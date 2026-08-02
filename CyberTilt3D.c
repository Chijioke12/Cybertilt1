#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define GRID_SIZE 7
#define CELL_SIZE 0.94f
#define NUM_LEVELS 4
#define TILT_MAX 0.24f
#define TILT_ACCEL 0.016f
#define TILT_FRICTION 0.88f
#define GRAVITY_CONSTANT 0.018f
#define BALL_FRICTION 0.985f
#define BOUNCE_DAMPING 0.55f
#define BALL_RADIUS 0.18f

typedef enum { SND_BOUNCE=0, SND_GEM, SND_FALL, SND_PORTAL, SND_WIN, SND_START, SND_COUNT } SndType;

// Levels: # wall, . floor, O hole, S spawn, * gem, E portal
static const char LEVELS[NUM_LEVELS][GRID_SIZE][8] = {
    {"#######","#E..*##","#.#.###","#*..S.#","#.###.#","#*...*#","#######"},
    {"#######","#E..*##","##.#..#","#*..S.#","#..#..#","#*..O*#","#######"},
    {"#######","#E.O.*#","#O.#.O#","#*..S.#","#O.#.O#","#*.O.*#","#######"},
    {"#######","#E.O.*#","##...##","#*.S.*#","##...##","#*.O.*#","#######"}
};

typedef struct { int x, z; } Hole;
typedef struct { Vector3 pos; int gx, gz; bool active; } Wall;
typedef struct { Vector3 pos; int gx, gz; bool collected; float rot; } Gem;

typedef struct {
    int score, highscore, level, gemsCollected, totalGems, lives;
    bool gameOver, gameWon, started, portalActive;
    float targetTiltX, targetTiltZ, currentTiltX, currentTiltZ;
    Vector3 ballPos, ballVel, ballRot;
    bool isFalling;
    float fallScale;
    Wall walls[64]; int wallCount;
    Gem gems[32]; int gemCount;
    Hole holes[32]; int holeCount;
    Vector3 portalPos; bool portalExists;
    float timeAcc;
} Game;

static Game G = {0};
static Sound sounds[SND_COUNT];
static Model gemModel;
static Model portalModel;
static Model ballModel;
static Model tileModel;
static Model wallModel;
static Vector3 stars[80];

void LoadHighscore(){ FILE*f=fopen("highscore.dat","r"); if(f){ fscanf(f,"%d",&G.highscore); fclose(f);} }
void SaveHighscore(){ if(G.score>G.highscore){ G.highscore=G.score; FILE*f=fopen("highscore.dat","w"); if(f){fprintf(f,"%d",G.highscore); fclose(f);} } }

void BuildLevel(){
    G.wallCount=0; G.gemCount=0; G.holeCount=0; G.gemsCollected=0; G.totalGems=0;
    G.portalActive=false; G.portalExists=false; G.isFalling=false; G.fallScale=1.0f;
    G.targetTiltX=G.targetTiltZ=G.currentTiltX=G.currentTiltZ=0;
    
    int li=(G.level-1)%NUM_LEVELS;
    for(int r=0;r<GRID_SIZE;r++){
        for(int c=0;c<GRID_SIZE;c++){
            char ch=LEVELS[li][r][c];
            float lx=(c-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float lz=(r-GRID_SIZE/2+0.5f)*CELL_SIZE;
            if(ch=='O'){ G.holes[G.holeCount++]=(Hole){c,r}; continue; }
            if(ch=='#'){ G.walls[G.wallCount++]=(Wall){{lx,0.25f,lz},c,r,true}; }
            else if(ch=='S'){ G.ballPos=(Vector3){lx,BALL_RADIUS,lz}; G.ballVel=(Vector3){0,0,0}; G.ballRot=(Vector3){0,0,0}; }
            else if(ch=='*'){ G.gems[G.gemCount++]=(Gem){{lx,0.28f,lz},c,r,false,0}; G.totalGems++; }
            else if(ch=='E'){ G.portalPos=(Vector3){lx,0.02f,lz}; G.portalExists=true; }
        }
    }
    printf("Level %d built: %d gems %d walls %d holes\n",G.level,G.totalGems,G.wallCount,G.holeCount);
}

void HandleFall(){
    if(G.lives>0) { if(IsSoundReady(sounds[SND_FALL])) PlaySound(sounds[SND_FALL]); }
    G.lives--;
    if(G.lives<=0){ G.gameOver=true; }
    else {
        int li=(G.level-1)%NUM_LEVELS;
        for(int r=0;r<GRID_SIZE;r++) for(int c=0;c<GRID_SIZE;c++) if(LEVELS[li][r][c]=='S'){
            float lx=(c-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float lz=(r-GRID_SIZE/2+0.5f)*CELL_SIZE;
            G.ballPos=(Vector3){lx,BALL_RADIUS,lz};
            G.ballVel=(Vector3){0,0,0};
            G.isFalling=false; G.fallScale=1.0f;
        }
    }
}

void TriggerEnter(){
    if(!G.started || G.gameOver || G.gameWon){
        G.started=true; G.gameOver=false; G.gameWon=false;
        G.level=1; G.score=0; G.lives=3;
        BuildLevel();
        if(IsSoundReady(sounds[SND_START])) PlaySound(sounds[SND_START]);
    }
}

void UpdateGame(float delta){
    if(!G.started || G.gameOver || G.gameWon) return;
    G.timeAcc+=delta;

    bool up=IsKeyDown(KEY_UP)||IsKeyDown(KEY_W)||IsKeyDown(KEY_KP_2);
    bool down=IsKeyDown(KEY_DOWN)||IsKeyDown(KEY_S)||IsKeyDown(KEY_KP_8);
    bool left=IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A)||IsKeyDown(KEY_KP_4);
    bool right=IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D)||IsKeyDown(KEY_KP_6);

    if(up) G.targetTiltX=fmaxf(-TILT_MAX,G.targetTiltX-TILT_ACCEL);
    if(down) G.targetTiltX=fminf(TILT_MAX,G.targetTiltX+TILT_ACCEL);
    if(left) G.targetTiltZ=fminf(TILT_MAX,G.targetTiltZ+TILT_ACCEL);
    if(right) G.targetTiltZ=fmaxf(-TILT_MAX,G.targetTiltZ-TILT_ACCEL);
    if(!up&&!down) G.targetTiltX*=TILT_FRICTION;
    if(!left&&!right) G.targetTiltZ*=TILT_FRICTION;

    G.currentTiltX+=(G.targetTiltX-G.currentTiltX)*0.16f;
    G.currentTiltZ+=(G.targetTiltZ-G.currentTiltZ)*0.16f;

    float halfGrid=(GRID_SIZE*CELL_SIZE)/2.0f;

    if(!G.isFalling){
        float ax=-sinf(G.currentTiltZ)*GRAVITY_CONSTANT;
        float az= sinf(G.currentTiltX)*GRAVITY_CONSTANT;
        G.ballVel.x+=ax; G.ballVel.z+=az;
        G.ballVel.x*=BALL_FRICTION; G.ballVel.z*=BALL_FRICTION;
        G.ballPos.x+=G.ballVel.x; G.ballPos.z+=G.ballVel.z;

        for(int i=0;i<G.wallCount;i++){
            float wx=(G.walls[i].gx-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float wz=(G.walls[i].gz-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float dx=G.ballPos.x-wx, dz=G.ballPos.z-wz;
            float ox=(CELL_SIZE/2+BALL_RADIUS)-fabsf(dx);
            float oz=(CELL_SIZE/2+BALL_RADIUS)-fabsf(dz);
            if(ox>0&&oz>0){
                if(ox<oz){ float s=dx>0?1:-1; G.ballPos.x=wx+s*(CELL_SIZE/2+BALL_RADIUS); G.ballVel.x*=-BOUNCE_DAMPING; if(fabsf(G.ballVel.x)>0.015f&&IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]); }
                else { float s=dz>0?1:-1; G.ballPos.z=wz+s*(CELL_SIZE/2+BALL_RADIUS); G.ballVel.z*=-BOUNCE_DAMPING; if(fabsf(G.ballVel.z)>0.015f&&IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]); }
            }
        }
        float bound=halfGrid-BALL_RADIUS;
        if(G.ballPos.x<-bound){G.ballPos.x=-bound; G.ballVel.x*=-BOUNCE_DAMPING; if(IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]);}
        if(G.ballPos.x> bound){G.ballPos.x= bound; G.ballVel.x*=-BOUNCE_DAMPING; if(IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]);}
        if(G.ballPos.z<-bound){G.ballPos.z=-bound; G.ballVel.z*=-BOUNCE_DAMPING; if(IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]);}
        if(G.ballPos.z> bound){G.ballPos.z= bound; G.ballVel.z*=-BOUNCE_DAMPING; if(IsSoundReady(sounds[SND_BOUNCE]))PlaySound(sounds[SND_BOUNCE]);}

        int gx=(int)floorf((G.ballPos.x+halfGrid)/CELL_SIZE);
        int gz=(int)floorf((G.ballPos.z+halfGrid)/CELL_SIZE);
        for(int i=0;i<G.holeCount;i++) if(G.holes[i].x==gx && G.holes[i].z==gz){
            float hx=(gx-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float hz=(gz-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float dx=G.ballPos.x-hx, dz=G.ballPos.z-hz;
            if(dx*dx + dz*dz < (CELL_SIZE*0.45f)*(CELL_SIZE*0.45f)){ G.isFalling=true; G.ballVel=(Vector3){0,0,0}; }
        }
        G.ballRot.z-=G.ballVel.x*2.2f;
        G.ballRot.x+=G.ballVel.z*2.2f;
    }else{
        G.fallScale-=0.05f; G.ballPos.y-=0.04f;
        if(G.fallScale<=0.05f) HandleFall();
    }

    for(int i=0;i<G.gemCount;i++){
        if(G.gems[i].collected) continue;
        G.gems[i].rot+=0.04f;
        float dx=G.ballPos.x-G.gems[i].pos.x, dy=G.ballPos.y-G.gems[i].pos.y, dz=G.ballPos.z-G.gems[i].pos.z;
        if(dx*dx+dy*dy+dz*dz < 0.44f*0.44f){
            G.gems[i].collected=true; G.gemsCollected++; G.score+=150;
            if(IsSoundReady(sounds[SND_GEM])) PlaySound(sounds[SND_GEM]);
            if(G.gemsCollected>=G.totalGems){ G.portalActive=true; if(IsSoundReady(sounds[SND_PORTAL])) PlaySound(sounds[SND_PORTAL]); }
        }
    }

    if(G.portalExists && G.portalActive){
        float dx=G.ballPos.x-G.portalPos.x, dz=G.ballPos.z-G.portalPos.z;
        if(dx*dx+dz*dz < 0.42f*0.42f){
            G.level++; G.score+=1000;
            if(IsSoundReady(sounds[SND_WIN])) PlaySound(sounds[SND_WIN]);
            if(G.level>NUM_LEVELS){ G.gameWon=true; }
            else BuildLevel();
        }
    }
    SaveHighscore();
}

int main(void){
    const int screenW=800, screenH=600;
    InitWindow(screenW, screenH, "CYBERTILT - 3D Balance Labyrinth (C + raylib)");
    InitAudioDevice();
    SetTargetFPS(60);

    LoadHighscore();
    G.level=1; G.lives=3;

    // Load sounds - will fallback silently if ogg files not found
    sounds[SND_BOUNCE]=LoadSound("audio/bounce.ogg");
    sounds[SND_GEM]=LoadSound("audio/gem.ogg");
    sounds[SND_FALL]=LoadSound("audio/fall.ogg");
    sounds[SND_PORTAL]=LoadSound("audio/portal.ogg");
    sounds[SND_WIN]=LoadSound("audio/win.ogg");
    sounds[SND_START]=LoadSound("audio/start.ogg");
    SetMasterVolume(1.8f); // masterVolumeBoost 180%

    // Create models
    Mesh tileMesh=GenMeshCube(CELL_SIZE*0.96f, 0.12f, CELL_SIZE*0.96f);
    tileModel=LoadModelFromMesh(tileMesh);
    Mesh wallMesh=GenMeshCube(CELL_SIZE*0.95f, 0.5f, CELL_SIZE*0.95f);
    wallModel=LoadModelFromMesh(wallMesh);
    Mesh ballMesh=GenMeshSphere(BALL_RADIUS, 20, 20);
    ballModel=LoadModelFromMesh(ballMesh);
    Mesh gemMesh=GenMeshSphere(0.15f, 8, 8); // will scale to look like octahedron via shader rotation
    gemModel=LoadModelFromMesh(gemMesh);
    Mesh torusMesh=GenMeshTorus(0.24f, 0.05f, 16, 16);
    portalModel=LoadModelFromMesh(torusMesh);

    Camera3D camera={0};
    camera.position=(Vector3){0,7.8f,6.8f};
    camera.target=(Vector3){0,-0.4f,0};
    camera.up=(Vector3){0,1,0};
    camera.fovy=46;
    camera.projection=CAMERA_PERSPECTIVE;

    BuildLevel();

    for(int i=0;i<80;i++){
        stars[i] = (Vector3){
            ((float)rand()/RAND_MAX-0.5f)*16,
            ((float)rand()/RAND_MAX-0.5f)*12,
            -(float)rand()/RAND_MAX*20
        };
    }

    while(!WindowShouldClose()){
        float dt=GetFrameTime();
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_5)) TriggerEnter();

        UpdateGame(dt);

        BeginDrawing();
        ClearBackground((Color){3,3,12,255});

        BeginMode3D(camera);

        // Starfield background (80 points)
        for(int i=0;i<80;i++){
            DrawSphere(stars[i], 0.02f, (Color){80,80,100,150});
        }

        rlPushMatrix();
        rlRotatef(G.currentTiltX*RAD2DEG, 1,0,0);
        rlRotatef(G.currentTiltZ*RAD2DEG, 0,0,1);

        int li=(G.level-1)%NUM_LEVELS;
        // Draw floor tiles
        for(int r=0;r<GRID_SIZE;r++) for(int c=0;c<GRID_SIZE;c++){
            char ch=LEVELS[li][r][c];
            if(ch=='O') continue;
            float lx=(c-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float lz=(r-GRID_SIZE/2+0.5f)*CELL_SIZE;
            DrawModel(tileModel, (Vector3){lx,-0.06f,lz}, 1.0f, (Color){7,7,24,255});
            DrawModelWires(tileModel, (Vector3){lx,-0.06f,lz}, 1.0f, (Color){17,17,51,255});
        }
        // Draw walls
        for(int i=0;i<G.wallCount;i++){
            DrawModel(wallModel, G.walls[i].pos, 1.0f, (Color){17,0,34,200});
            DrawModelWires(wallModel, G.walls[i].pos, 1.0f, (Color){255,0,255,255});
        }
        // Draw holes as black pits
        for(int i=0;i<G.holeCount;i++){
            float lx=(G.holes[i].x-GRID_SIZE/2+0.5f)*CELL_SIZE;
            float lz=(G.holes[i].z-GRID_SIZE/2+0.5f)*CELL_SIZE;
            DrawCube((Vector3){lx,-0.1f,lz}, CELL_SIZE, 0.05f, CELL_SIZE, BLACK);
            DrawCubeWires((Vector3){lx,-0.1f,lz}, CELL_SIZE, 0.05f, CELL_SIZE, (Color){255,0,80,255});
        }
        // Draw gems
        for(int i=0;i<G.gemCount;i++) if(!G.gems[i].collected){
            rlPushMatrix();
            rlTranslatef(G.gems[i].pos.x, G.gems[i].pos.y, G.gems[i].pos.z);
            rlRotatef(G.gems[i].rot*RAD2DEG*2, 0,1,0);
            rlRotatef(G.gems[i].rot*30, 1,0,0);
            DrawModel(gemModel, (Vector3){0,0,0}, 1.0f, (Color){0,255,204,255});
            rlPopMatrix();
        }
        // Draw portal
        if(G.portalExists){
            float pulse = G.portalActive ? 1.0f+sinf(G.timeAcc*5)*0.1f : 0.8f;
            Color pc = G.portalActive ? (Color){255,204,0,255} : (Color){100,80,0,180};
            rlPushMatrix();
            rlTranslatef(G.portalPos.x, G.portalPos.y, G.portalPos.z);
            rlScalef(pulse,pulse,pulse);
            rlRotatef(90,1,0,0);
            DrawModel(portalModel, (Vector3){0,0,0}, 1.0f, pc);
            rlPopMatrix();
            if(G.portalActive){
                DrawPoint3D(G.portalPos, (Color){255,170,0,255});
            }
        }
        // Draw ball
        {
            float s = G.isFalling ? fmaxf(0.001f,G.fallScale) : 1.0f;
            rlPushMatrix();
            rlTranslatef(G.ballPos.x, G.ballPos.y, G.ballPos.z);
            rlRotatef(G.ballRot.x*RAD2DEG, 1,0,0);
            rlRotatef(G.ballRot.z*RAD2DEG, 0,0,1);
            rlScalef(s,s,s);
            DrawModel(ballModel, (Vector3){0,0,0}, 1.0f, (Color){0,255,255,255});
            rlPopMatrix();
            // ball glow light
            DrawSphere((Vector3){G.ballPos.x,1.2f,G.ballPos.z},0.05f, (Color){0,255,255,100});
        }

        rlPopMatrix();
        EndMode3D();

        // UI - Top bar
        DrawRectangle(0,0,screenW,30,(Color){3,3,12,220});
        DrawLine(0,30,screenW,30, (Color){255,0,255,255});
        DrawText(TextFormat("LVL:%d",G.level),10,7,20,MAGENTA);
        if(G.portalActive) DrawText("ESCAPE NOW!", screenW/2-70,7,20,YELLOW);
        else DrawText(TextFormat("CRYS:%d/%d",G.gemsCollected,G.totalGems), screenW/2-60,7,20,SKYBLUE);
        DrawText(TextFormat("Lives: %d  Score:%d  Best:%d",G.lives,G.score,G.highscore), screenW-260,7,16,WHITE);

        if(!G.started){
            DrawRectangle(screenW/2-110, screenH/2-80, 220,160,(Color){3,3,12,240});
            DrawRectangleLines(screenW/2-110, screenH/2-80, 220,160, SKYBLUE);
            DrawText("CYBERTILT", screenW/2-70, screenH/2-70,24,SKYBLUE);
            DrawText("3D BALANCE LABYRINTH", screenW/2-80, screenH/2-45,10,PINK);
            DrawText("Arrows/WASD: Tilt", screenW/2-70, screenH/2-20,12,GRAY);
            DrawText("Collect * then go to E", screenW/2-75, screenH/2-5,12,GRAY);
            DrawText("Avoid O holes!", screenW/2-55, screenH/2+10,12,GRAY);
            if(GuiButton((Rectangle){screenW/2-50,screenH/2+35,100,30},"POWER UP") || IsKeyPressed(KEY_ENTER)) TriggerEnter();
        } else if(G.gameOver){
            DrawRectangle(screenW/2-110, screenH/2-60, 220,120,(Color){20,0,0,240});
            DrawRectangleLines(screenW/2-110, screenH/2-60, 220,120, RED);
            DrawText("VOID DROP", screenW/2-60, screenH/2-45,24,RED);
            DrawText(TextFormat("Score: %d",G.score), screenW/2-40, screenH/2-15,16,WHITE);
            if(GuiButton((Rectangle){screenW/2-40,screenH/2+10,80,30},"RETRY")) TriggerEnter();
        } else if(G.gameWon){
            DrawRectangle(screenW/2-120, screenH/2-60, 240,120,(Color){20,20,0,240});
            DrawRectangleLines(screenW/2-120, screenH/2-60, 240,120, GOLD);
            DrawText("GRAND MASTER", screenW/2-85, screenH/2-45,22,GOLD);
            DrawText(TextFormat("Score: %d",G.score), screenW/2-40, screenH/2-15,16,WHITE);
            if(GuiButton((Rectangle){screenW/2-60,screenH/2+10,120,30},"EXTRACT AGAIN")) TriggerEnter();
        }

        EndDrawing();
    }

    UnloadModel(tileModel); UnloadModel(wallModel); UnloadModel(ballModel); UnloadModel(gemModel); UnloadModel(portalModel);
    for(int i=0;i<SND_COUNT;i++) if(IsSoundReady(sounds[i])) UnloadSound(sounds[i]);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

// Minimal GuiButton implementation if raygui not included
#ifndef RAYGUI_H
bool GuiButton(Rectangle bounds, const char* text){
    bool pressed=false;
    Vector2 mouse=GetMousePosition();
    bool hover=CheckCollisionPointRec(mouse,bounds);
    Color col= hover? SKYBLUE : (Color){0,255,255,255};
    if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) pressed=true;
    DrawRectangleRec(bounds, col);
    DrawRectangleLinesEx(bounds,2, (Color){0,100,100,255});
    int tw=MeasureText(text,12);
    DrawText(text, bounds.x+bounds.width/2-tw/2, bounds.y+bounds.height/2-6,12,BLACK);
    return pressed;
}
#endif
