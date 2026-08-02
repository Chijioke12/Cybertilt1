import { useEffect, useState, useRef } from 'preact/hooks';

export default function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [loaded, setLoaded] = useState(false);
  const [statusText, setStatusText] = useState("Initializing Web/KaiOS environment...");
  const [logs, setLogs] = useState<string[]>([]);
  const [showConsole, setShowConsole] = useState(true);

  // Note: Direct keyboard events are handled by the Emscripten/Raylib listener.
  // Physical key presses on devices and laptops are standard browser events.

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    // Define the global Module object required by Emscripten output
    (window as any).Module = {
      canvas: canvas,
      print: (text: string) => {
        console.log("[Raylib stdout]", text);
        setLogs(prev => [...prev.slice(-39), `[STDOUT] ${text}`]);
      },
      printErr: (text: string) => {
        console.error("[Raylib stderr]", text);
        setLogs(prev => [...prev.slice(-39), `[STDERR] ${text}`]);
      },
      setStatus: (status: string) => {
        if (!status) {
          setLoaded(true);
          setStatusText("Game Active");
        } else {
          // Keep track of the downloading/compiling status text
          setStatusText(status);
        }
      },
      onRuntimeInitialized: () => {
        setLoaded(true);
        setStatusText("Ready to Play!");
        console.log("CyberTilt 3D Raylib Runtime Initialized");
      },
      locateFile: (path: string, prefix: string) => {
        return prefix + path;
      }
    };

    // Inject game.js compiled file dynamically
    const script = document.createElement('script');
    script.src = '/game.js';
    script.async = true;
    document.body.appendChild(script);

    return () => {
      // Clean up when the runner component unmounts
      if (document.body.contains(script)) {
        document.body.removeChild(script);
      }
      delete (window as any).Module;
    };
  }, []);

  return (
    <div id="app" className="min-h-screen w-full bg-slate-950 text-slate-100 flex flex-col p-4 md:p-6 lg:p-8">
      {/* HEADER SECTION */}
      <header className="max-w-7xl mx-auto w-full mb-6 flex flex-col md:flex-row md:items-center md:justify-between border-b border-slate-800 pb-4">
        <div>
          <div className="flex items-center gap-3">
            <span className="px-2.5 py-0.5 text-[10px] font-bold tracking-wider uppercase bg-cyan-500/10 text-cyan-400 rounded border border-cyan-500/20">
              C & Raylib Engine
            </span>
            <span className="px-2.5 py-0.5 text-[10px] font-bold tracking-wider uppercase bg-yellow-500/10 text-yellow-400 rounded border border-yellow-500/20">
              KaiOS Pak Build
            </span>
          </div>
          <h1 className="text-2xl md:text-3xl font-extrabold tracking-tight mt-1 text-slate-100">
            CyberTilt 3D <span className="font-light text-slate-400 text-lg">Web Runner</span>
          </h1>
        </div>
        <div className="mt-2 md:mt-0 flex items-center gap-4">
          <div className="flex items-center gap-2 text-xs text-slate-400">
            <span className={`w-2.5 h-2.5 rounded-full ${loaded ? 'bg-emerald-500 animate-pulse' : 'bg-yellow-500'}`} />
            {statusText}
          </div>
        </div>
      </header>

      {/* MAIN TWO-COLUMN LAYOUT */}
      <main className="max-w-7xl mx-auto w-full grid grid-cols-1 lg:grid-cols-12 gap-6 items-start flex-grow">
        
        {/* LEFT COLUMN: ACTIVE CANVAS + INTERACTIVE D-PAD */}
        <section className="lg:col-span-8 flex flex-col gap-6 w-full">
          
          {/* EMULATOR VIEW FRAME */}
          <div className="relative bg-slate-900 rounded-xl border border-slate-800 overflow-hidden shadow-2xl p-4 flex flex-col items-center">
            
            <div className="w-full flex items-center justify-between mb-3 text-xs text-slate-400 px-1 border-b border-slate-800 pb-2">
              <span className="font-mono text-cyan-400">⚡ DIRECT RENDERING CONSOLE</span>
              <span className="font-light">800 × 600 Viewport</span>
            </div>

            {/* THE CANVAS WHERE EMCOSM COMPILED RAYLIB GAME OUTPUTS */}
            <div className="relative w-full max-w-[800px] aspect-[4/3] bg-black rounded-lg overflow-hidden border border-slate-950 shadow-inner">
              <canvas
                id="canvas"
                ref={canvasRef}
                onContextMenu={(e) => e.preventDefault()}
                className="w-full h-full block object-contain"
              />
              
              {/* LOADING SCREEN UNDERLAY */}
              {!loaded && (
                <div className="absolute inset-0 bg-slate-950 flex flex-col items-center justify-center p-6 text-center z-10">
                  <div className="w-12 h-12 border-4 border-cyan-500/30 border-t-cyan-400 rounded-full animate-spin mb-4" />
                  <h3 className="text-md font-bold tracking-wide text-slate-200 uppercase mb-2">
                    Running asm.js Virtual Machine
                  </h3>
                  <p className="text-xs text-slate-400 max-w-sm">
                    {statusText}...
                  </p>
                  <p className="text-[11px] text-slate-500 mt-6 max-w-xs italic">
                    Note: Using a custom-built low-latency Gecko-compatible runtime pipeline.
                  </p>
                </div>
              )}
            </div>

            {/* SHORTCUT BAR */}
            <div className="w-full flex justify-between gap-3 mt-4 px-1">
              <span className="text-xs text-slate-500 font-mono">Input: Use standard physical keypad or browser arrow/enter keys.</span>
              <button
                onClick={() => setShowConsole(!showConsole)}
                className="text-xs text-cyan-400 hover:text-cyan-300 font-mono transition-colors"
              >
                {showConsole ? "Hide VM Console ✕" : "Show VM Console ⚡"}
              </button>
            </div>
          </div>

          {/* REALTIME VM STDOUT LOGGER */}
          {showConsole && (
            <div className="bg-slate-900 border border-slate-800 rounded-xl p-4 shadow-md w-full">
              <div className="flex items-center justify-between mb-2 pb-2 border-b border-slate-800">
                <span className="text-xs font-bold text-cyan-400 font-mono">⚡ LIVE ASM.JS STDOUT CONSOLE</span>
                <button
                  onClick={() => setLogs([])}
                  className="text-[10px] text-slate-500 hover:text-slate-300 underline font-mono"
                >
                  Clear logs
                </button>
              </div>
              <div className="bg-slate-950 rounded p-3 h-40 overflow-y-auto font-mono text-[11px] text-emerald-400/90 leading-relaxed border border-slate-950">
                {logs.length === 0 ? (
                  <span className="text-slate-600 italic">No output streams captured yet. Launch game to log runtime stats...</span>
                ) : (
                  logs.map((log, i) => (
                    <div key={i} className="whitespace-pre-wrap border-b border-slate-900/50 pb-0.5 last:border-0">
                      {log}
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

        </section>

        {/* RIGHT COLUMN: KAIOS DOCUMENTATION & MANUALS */}
        <aside className="lg:col-span-4 flex flex-col gap-6 w-full">
          
          {/* THE GAME MANUAL */}
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-lg">
            <h2 className="text-sm font-bold text-cyan-400 flex items-center gap-2 border-b border-slate-800 pb-3 uppercase tracking-wider">
              🎯 Game Instructions
            </h2>
            <ul className="flex flex-col gap-3.5 pl-4 list-disc text-xs text-slate-300 mt-4 leading-relaxed">
              <li>
                <b>Control Labyrinth:</b> Use the <b>Arrow keys</b> (or virtual D-Pad) to tip the game board in 3D.
              </li>
              <li>
                <b>Physics Mechanics:</b> Gravity pulls the ball along the tipped board. Keep your balance!
              </li>
              <li>
                <b>Objective:</b> Collect all gold crystals (<b className="text-yellow-400">*</b>) on the map.
              </li>
              <li>
                <b>Escape Portal:</b> Once collected, the exit portal (<b className="text-orange-400">E</b>) activates. Roll inside to level up!
              </li>
              <li>
                <b>Hazards:</b> Stay away from black holes (<b className="text-rose-500">O</b>). Dropping in resets the level and costs a life.
              </li>
            </ul>
          </div>

          {/* KAIOS PROFILE SPEC */}
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-lg">
            <h2 className="text-sm font-bold text-pink-400 flex items-center gap-2 border-b border-slate-800 pb-3 uppercase tracking-wider">
              📱 KaiOS Target Device Specification
            </h2>
            <div className="flex flex-col gap-3 mt-4 text-xs text-slate-300 leading-relaxed">
              <p>
                KaiOS targets lightweight feature phones running <b>Gecko 48 (Firefox 48)</b>. It has very strict resource and compatibility limits.
              </p>
              
              <div className="flex justify-between py-1.5 border-b border-slate-800/40 text-[11px]">
                <span className="text-slate-500">WASM Support:</span>
                <span className="text-rose-400 font-bold uppercase">Disabled (Using asm.js)</span>
              </div>
              <div className="flex justify-between py-1.5 border-b border-slate-800/40 text-[11px]">
                <span className="text-slate-500">Javascript Engine:</span>
                <span className="text-cyan-400 font-bold">SpiderMonkey 48</span>
              </div>
              <div className="flex justify-between py-1.5 border-b border-slate-800/40 text-[11px]">
                <span className="text-slate-500">ES6 Support:</span>
                <span className="text-yellow-400 font-bold">Partial</span>
              </div>

              <div className="bg-slate-950/80 border border-slate-800/60 rounded p-3 mt-2">
                <span className="font-bold text-cyan-400 block text-[10px] uppercase font-mono mb-1">
                  ⚙️ Emscripten Command:
                </span>
                <code className="text-[10px] text-cyan-200/80 font-mono word-break-all block break-all leading-normal bg-black/40 p-1 rounded">
                  emcc CyberTilt3D.c -o public/game.js -Os -Wall -I./raylib/src/ -L./raylib/src/ -lraylib -s USE_GLFW=3 -s WASM=0 -s LEGACY_VM_SUPPORT=1 --memory-init-file 0
                </code>
              </div>
            </div>
          </div>

          {/* SIDELOADING GUIDE */}
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-lg">
            <h2 className="text-sm font-bold text-yellow-400 flex items-center gap-2 border-b border-slate-800 pb-3 uppercase tracking-wider">
              🔧 Sideloading Guide
            </h2>
            <div className="flex flex-col gap-3 mt-4 text-xs text-slate-300 leading-relaxed">
              <p>To load CyberTilt 3D onto real KaiOS handsets (e.g., Nokia 2720 Flip, Nokia 800 Tough, JioPhone):</p>
              <ol className="pl-4 list-decimal flex flex-col gap-2 text-slate-300">
                <li>
                  Retrieve the single compiled build asset: <code className="text-yellow-400 font-mono">game.js</code>.
                </li>
                <li>
                  Create a standard <code className="text-cyan-400 font-mono">manifest.webapp</code> file specifying device portrait/landscape orientation, launcher icons, and permissions.
                </li>
                <li>
                  Connect your handset via microUSB and enable developer mode debugging (dial <code className="text-cyan-400 font-mono">*#*#33284#*#*</code> on keypad).
                </li>
                <li>
                  Open <b>WebIDE</b> (Waterfox, Pale Moon, or old Firefox) or use the <b>gPhone sideload CLI</b>.
                </li>
                <li>
                  Install the package directly onto your phone and experience full 3D labyrinth physics anywhere!
                </li>
              </ol>
            </div>
          </div>

        </aside>
      </main>
    </div>
  );
}
