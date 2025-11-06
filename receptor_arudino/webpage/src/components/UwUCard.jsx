import { useEffect, useState } from 'react';

const faces = ['UwU', 'OwO'];
const intervalMs = 2200;

function UwUCard() {
  const [index, setIndex] = useState(0);

  useEffect(() => {
    const id = setInterval(() => {
      setIndex((prev) => (prev + 1) % faces.length);
    }, intervalMs);

    return () => {
      clearInterval(id);
    };
  }, []);

  const display = faces[index];

  return (
    <section className="sensor-card sensor-card--uwu" id="terminal">
      <div className="uwu-card" aria-hidden="true">
        <div className="uwu-card__bubble">
          <span key={display} className="uwu-card__text">
            {display}
          </span>
        </div>
      </div>
      <span className="sr-only" aria-live="polite">
        {display}
      </span>
    </section>
  );
}

export default UwUCard;
