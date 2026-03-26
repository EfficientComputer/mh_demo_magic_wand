import logo from '../assets/logo.svg';

interface StartProps {
  isWandConnected: boolean;
  onStart: () => void;
}

function Start({ isWandConnected, onStart }: StartProps) {
  return (
    <section className="w-full flex flex-col items-center">
      <img src={logo} alt="Logo" className="h-88" />

      <button className="btn" id="startBtn" onClick={onStart} disabled={!isWandConnected}>
        Start Game
      </button>

      <div className="w-full mt-12 p-6 rounded-xl flex justify-between items-center gap-4 flex-wrap">
        <div className="flex items-center">
          <div
            className={`w-3 h-3 rounded-full mr-3 transition-all duration-300 ${isWandConnected
              ? 'bg-[#00ff88] shadow-[0_0_10px_rgba(0,255,136,0.6)]'
              : 'bg-[#ff0000] shadow-[0_0_10px_rgba(255,0,0,0.6)]'
              }`}
            id="wandStatus"
          ></div>
          <span id="wandStatusText" className="text-lg text-[var(--color-text)]">
            {isWandConnected ? 'Wand Connected' : 'Wand Disconnected'}
          </span>
        </div>
        <div className="text-lg text-[var(--color-text)]">
          CES 2026 • Magic Wand Demo
        </div>
      </div>
    </section>
  );
}

export default Start;
