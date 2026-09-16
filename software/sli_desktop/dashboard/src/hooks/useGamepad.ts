import { useEffect, useRef, useCallback } from "react";

const API_BASE = "http://localhost:8000/api";

export type AxisId = 1 | 2 | 3 | 4; // 1=Swing, 2=Lift, 3=Tele, 4=Winch

interface UseGamepadReturn {
  /** Start a gamepad polling loop. Call once on mount. */
  startPolling: () => void;
  stopPolling: () => void;
}

/**
 * useGamepad — polls the W3C Gamepad API and maps Xbox controller
 * axes + buttons to motor G-code commands sent to the backend.
 *
 * Mapping:
 *   Left Stick X   → Swing   (M1)
 *   Left Stick Y   → Boom Lift (M2)
 *   Right Stick Y  → Telescope (M3)
 *   Right Trigger  → Winch Down (M4 D1)
 *   Left Trigger   → Winch Up  (M4 D0)
 *   B Button       → E-Stop (T0)
 *
 * Deadzone: 0.15 (configurable)
 * Speed: axis value (0.0–1.0) × 255, sent raw — ESP32 is the brain.
 */
export function useGamepad(
  enabled: boolean,
  deadzone: number = 0.15
): UseGamepadReturn {
  const rafRef = useRef<number | null>(null);
  const activeAxes = useRef<Map<AxisId, number>>(new Map());

  const sendCommand = useCallback(async (cmd: string) => {
    try {
      await fetch(`${API_BASE}/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ command: cmd }),
      });
    } catch {
      // Silently ignore — not connected
    }
  }, []);

  const applyDeadzone = (value: number): number => {
    return Math.abs(value) < deadzone ? 0 : value;
  };

  const axisToSpeed = (value: number): number => {
    return Math.round(Math.abs(value) * 255);
  };

  const pollGamepad = useCallback(() => {
    const gamepads = navigator.getGamepads();
    const gp = gamepads[0]; // Use first connected gamepad

    if (gp) {
      // Read axes with deadzone
      const leftX  = applyDeadzone(gp.axes[0]);  // Swing
      const leftY  = applyDeadzone(gp.axes[1]);  // Boom Lift
      const rightY = applyDeadzone(gp.axes[3]);  // Telescope
      const lTrigger = gp.buttons[6]?.value ?? 0; // Left trigger (winch up)
      const rTrigger = gp.buttons[7]?.value ?? 0; // Right trigger (winch down)
      const bButton  = gp.buttons[1]?.pressed ?? false; // B = E-stop

      // E-STOP button — highest priority
      if (bButton) {
        sendCommand("T0");
      }

      // Swing (Axis 1)
      handleAxis(1, leftX, leftX > 0 ? 1 : 0);
      // Boom Lift (Axis 2) — Y axis inverted (up = negative on stick)
      handleAxis(2, leftY, leftY > 0 ? 1 : 0);
      // Telescope (Axis 3)
      handleAxis(3, rightY, rightY > 0 ? 1 : 0);

      // Winch (Axis 4) — use triggers
      const winchValue = rTrigger > deadzone ? rTrigger : (lTrigger > deadzone ? -lTrigger : 0);
      handleAxis(4, winchValue, winchValue > 0 ? 1 : 0);
    }

    rafRef.current = requestAnimationFrame(pollGamepad);
  }, [sendCommand, deadzone]);

  const handleAxis = (
    axis: AxisId,
    value: number,
    direction: 0 | 1
  ) => {
    const speed = axisToSpeed(value);
    const prevSpeed = activeAxes.current.get(axis) ?? 0;

    if (speed === 0 && prevSpeed !== 0) {
      // Axis returned to center — stop it
      sendCommand(`M0 A${axis}`);
      activeAxes.current.set(axis, 0);
    } else if (speed > 0) {
      // Send only if speed changed meaningfully (> 5 units)
      if (Math.abs(speed - prevSpeed) > 5) {
        sendCommand(`M${axis} S${speed} D${direction}`);
        activeAxes.current.set(axis, speed);
      }
    }
  };

  const startPolling = useCallback(() => {
    if (!enabled) return;
    rafRef.current = requestAnimationFrame(pollGamepad);
  }, [enabled, pollGamepad]);

  const stopPolling = useCallback(() => {
    if (rafRef.current !== null) {
      cancelAnimationFrame(rafRef.current);
      rafRef.current = null;
    }
    // Stop all active axes
    activeAxes.current.forEach((_, axis) => {
      sendCommand(`M0 A${axis}`);
    });
    activeAxes.current.clear();
  }, [sendCommand]);

  useEffect(() => {
    return () => stopPolling();
  }, [stopPolling]);

  return { startPolling, stopPolling };
}
