import { useEffect, useRef, useState } from 'react';

function TelemetryTerminal({ messages, interval = 2000 }) {
  const [buffer, setBuffer] = useState([]);
  const indexRef = useRef(0);
  const timerRef = useRef();

  useEffect(() => {
    setBuffer([]);
    indexRef.current = 0;
  }, [messages]);

  useEffect(() => {
    if (!Array.isArray(messages) || messages.length === 0) {
      return () => undefined;
    }

    const pushLine = () => {
      setBuffer((prev) => {
        const nextMessage = messages[indexRef.current % messages.length];
        indexRef.current += 1;
        const next = [...prev, nextMessage];
        if (next.length > 10) {
          next.shift();
        }
        return next;
      });
    };

    pushLine();
    timerRef.current = window.setInterval(pushLine, interval);

    return () => {
      if (timerRef.current) {
        window.clearInterval(timerRef.current);
        timerRef.current = undefined;
      }
    };
  }, [messages, interval]);

  return (
    <div className="terminal" role="log" aria-live="polite">
      <div className="terminal__chrome">
        <span />
        <span />
        <span />
        <p>telemetria.log</p>
      </div>
      <div className="terminal__body">
        {buffer.map((line, index) => (
          <p key={`${line}-${index}`} className="terminal__line">
            <span className="terminal__prompt">$</span> {line}
          </p>
        ))}
      </div>
    </div>
  );
}

export default TelemetryTerminal;
