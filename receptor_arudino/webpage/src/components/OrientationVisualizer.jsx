import { useEffect, useMemo, useRef, useState } from 'react';

const clampAngle = (value) => {
  if (value == null || Number.isNaN(value)) {
    return 0;
  }
  return value;
};

const wrapAngle = (value) => {
  const wrapped = ((value + 180) % 360 + 360) % 360 - 180;
  return wrapped;
};

const interpolateAngle = (from, to, progress) => {
  const start = wrapAngle(from);
  const end = wrapAngle(to);
  const delta = ((end - start + 540) % 360) - 180;
  return wrapAngle(start + delta * progress);
};

function OrientationVisualizer({ pitch, roll, yaw }) {
  const targetOrientation = useMemo(() => {
    return {
      pitch: clampAngle(pitch),
      roll: clampAngle(roll),
      yaw: clampAngle(yaw),
    };
  }, [pitch, roll, yaw]);

  const [displayOrientation, setDisplayOrientation] = useState(targetOrientation);
  const animationRef = useRef();
  const stateRef = useRef(displayOrientation);

  useEffect(() => {
    stateRef.current = displayOrientation;
  }, [displayOrientation]);

  useEffect(() => {
    const start = stateRef.current;
    const target = targetOrientation;

    const pitchDelta = Math.abs(((target.pitch - start.pitch + 540) % 360) - 180);
    const rollDelta = Math.abs(((target.roll - start.roll + 540) % 360) - 180);
    const yawDelta = Math.abs(((target.yaw - start.yaw + 540) % 360) - 180);

    if (pitchDelta < 0.001 && rollDelta < 0.001 && yawDelta < 0.001) {
      setDisplayOrientation(target);
      stateRef.current = target;
      return () => undefined;
    }

    const duration = 680;
    let startTime;

    const step = (timestamp) => {
      if (startTime == null) {
        startTime = timestamp;
      }
      const elapsed = timestamp - startTime;
      const t = Math.min(1, elapsed / duration);
      const eased = 1 - Math.pow(1 - t, 3);

      const next = {
        pitch: interpolateAngle(start.pitch, target.pitch, eased),
        roll: interpolateAngle(start.roll, target.roll, eased),
        yaw: interpolateAngle(start.yaw, target.yaw, eased),
      };

      stateRef.current = next;
      setDisplayOrientation(next);

      if (t < 1) {
        animationRef.current = window.requestAnimationFrame(step);
      }
    };

    animationRef.current = window.requestAnimationFrame(step);

    return () => {
      if (animationRef.current) {
        window.cancelAnimationFrame(animationRef.current);
        animationRef.current = undefined;
      }
    };
  }, [targetOrientation]);

  const cubeTransform = useMemo(() => {
    const x = -displayOrientation.pitch;
    const y = displayOrientation.yaw;
    const z = displayOrientation.roll;
    return `rotateX(${x}deg) rotateY(${y}deg) rotateZ(${z}deg)`;
  }, [displayOrientation.pitch, displayOrientation.roll, displayOrientation.yaw]);

  return (
    <div className="chart chart--orientation orientation">
      <h5 className="chart__title">Actitud 3D</h5>
      <div className="orientation__scene" role="img" aria-label="Representación tridimensional de la actitud">
        <div className="orientation__cube" style={{ transform: cubeTransform }}>
          <span className="orientation__face orientation__face--front">N</span>
          <span className="orientation__face orientation__face--back">S</span>
          <span className="orientation__face orientation__face--right">E</span>
          <span className="orientation__face orientation__face--left">O</span>
          <span className="orientation__face orientation__face--top">Arriba</span>
          <span className="orientation__face orientation__face--bottom">Abajo</span>
        </div>
        <div className="orientation__axis orientation__axis--x" aria-hidden />
        <div className="orientation__axis orientation__axis--y" aria-hidden />
        <div className="orientation__axis orientation__axis--z" aria-hidden />
      </div>
      <dl className="orientation__legend">
        <div>
          <dt>Pitch</dt>
          <dd>{displayOrientation.pitch.toFixed(2)}°</dd>
        </div>
        <div>
          <dt>Roll</dt>
          <dd>{displayOrientation.roll.toFixed(2)}°</dd>
        </div>
        <div>
          <dt>Yaw</dt>
          <dd>{displayOrientation.yaw.toFixed(2)}°</dd>
        </div>
      </dl>
    </div>
  );
}

export default OrientationVisualizer;
