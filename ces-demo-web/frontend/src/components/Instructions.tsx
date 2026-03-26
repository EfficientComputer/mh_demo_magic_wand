interface InstructionsProps {
  onContinue: () => void;
}

function Instructions({ onContinue }: InstructionsProps) {
  return (
    <section>
      <h1>How to Play</h1>
      <ol className="list-decimal list-inside p-0 my-6 mx-0 grid gap-3.5 text-xl">
        <li>Watch the countdown and get ready.</li>
        <li>Mimic the gesture shown on screen.</li>
        <li>Hold the wand still when prompted.</li>
        <li>See your score and power stats at the end.</li>
      </ol>
      <div className="py-5 px-6 rounded-md border border-[var(--color-border)] italic text-xl" style={{ color: 'var(--color-blue-light)' }}>
        Tips: Keep motions smooth. Hold the wand switch side up. Maintain a relaxed grip.
      </div>
      <div className="center mt-8">
        <button className="btn" onClick={onContinue}>Continue</button>
      </div>
    </section>
  );
}

export default Instructions;
