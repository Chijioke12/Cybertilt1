export default function App() {
  return (
    <main style={{
      maxWidth: '480px',
      padding: '2.5rem',
      backgroundColor: '#ffffff',
      borderRadius: '12px',
      boxShadow: '0 4px 12px rgba(0, 0, 0, 0.05)',
      textAlign: 'center'
    }}>
      <h1 style={{ margin: '0 0 1rem 0', fontSize: '1.75rem', color: '#0f172a' }}>
        Preact App
      </h1>
      <p style={{ margin: 0, color: '#475569', fontSize: '1rem' }}>
        Successfully uninstalled React and Tailwind CSS, and migrated to Preact.
      </p>
    </main>
  );
}
