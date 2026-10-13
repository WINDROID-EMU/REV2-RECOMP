package com.capcom.rerev2;

import android.app.NativeActivity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import com.windroid.emu.views.VirtualControllerInputView;

public class GameActivity extends NativeActivity {

    private static final String TAG = "NFS-GameActivity";

    // Native XInput bitmask constants
    public static final int BTN_DPAD_UP        = 0x0001;
    public static final int BTN_DPAD_DOWN      = 0x0002;
    public static final int BTN_DPAD_LEFT      = 0x0004;
    public static final int BTN_DPAD_RIGHT     = 0x0008;
    public static final int BTN_START          = 0x0010;
    public static final int BTN_BACK           = 0x0020;
    public static final int BTN_LEFT_THUMB     = 0x0040;
    public static final int BTN_RIGHT_THUMB    = 0x0080;
    public static final int BTN_LEFT_SHOULDER  = 0x0100;
    public static final int BTN_RIGHT_SHOULDER = 0x0200;
    public static final int BTN_A              = 0x1000;
    public static final int BTN_B              = 0x2000;
    public static final int BTN_X              = 0x4000;
    public static final int BTN_Y              = 0x8000;

    public static native void nativeSetVirtualGamepad(
            int buttons, float thumbLx, float thumbLy, float thumbRx, float thumbRy, float leftTrigger, float rightTrigger);

    /** Returns engine FPS measured from VdSwap presents (0 until the game renders its first frame). */
    public static native float nativeGetEngineFps();

    private VirtualControllerInputView controlsView;
    private PopupWindow controlsPopup;
    private InputManager inputManager;
    private InputManager.InputDeviceListener inputDeviceListener;

    // ── Engine FPS overlay ────────────────────────────────────────────────────
    private TextView    fpsTextView;
    private Handler     fpsPollHandler;
    private Runnable    fpsPollRunnable;
    private static final long FPS_POLL_INTERVAL_MS = 500L;
    // ─────────────────────────────────────────────────────────────────────────

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "GameActivity onCreate with Windroid-emu virtual controls");

        configureFullscreen();

        // Create VirtualControllerInputView from Windroid-emu
        controlsView = new VirtualControllerInputView(this);
        controlsView.setBackgroundColor(Color.TRANSPARENT);
        controlsView.setAlpha(0.85F);

        // Map Windroid-emu virtual controller state to native XInput gamepad
        controlsView.setControllerListener((lx, ly, rx, ry, lt, rt, buttonsA, buttonsB, dpadStatus) -> {
            int xinputButtons = 0;

            if ((buttonsA & VirtualControllerInputView.MASK_A) != 0)  xinputButtons |= BTN_A;
            if ((buttonsA & VirtualControllerInputView.MASK_B) != 0)  xinputButtons |= BTN_B;
            if ((buttonsA & VirtualControllerInputView.MASK_X) != 0)  xinputButtons |= BTN_X;
            if ((buttonsA & VirtualControllerInputView.MASK_Y) != 0)  xinputButtons |= BTN_Y;
            if ((buttonsA & VirtualControllerInputView.MASK_RB) != 0) xinputButtons |= BTN_RIGHT_SHOULDER;
            if ((buttonsA & VirtualControllerInputView.MASK_LB) != 0) xinputButtons |= BTN_LEFT_SHOULDER;
            if ((buttonsA & VirtualControllerInputView.MASK_LS) != 0) xinputButtons |= BTN_LEFT_THUMB;
            if ((buttonsA & VirtualControllerInputView.MASK_RS) != 0) xinputButtons |= BTN_RIGHT_THUMB;

            if ((buttonsB & VirtualControllerInputView.MASK_START) != 0)  xinputButtons |= BTN_START;
            if ((buttonsB & VirtualControllerInputView.MASK_SELECT) != 0) xinputButtons |= BTN_BACK;

            switch (dpadStatus) {
                case 1: xinputButtons |= BTN_DPAD_UP; break;
                case 2: xinputButtons |= (BTN_DPAD_UP | BTN_DPAD_RIGHT); break;
                case 3: xinputButtons |= BTN_DPAD_RIGHT; break;
                case 4: xinputButtons |= (BTN_DPAD_DOWN | BTN_DPAD_RIGHT); break;
                case 5: xinputButtons |= BTN_DPAD_DOWN; break;
                case 6: xinputButtons |= (BTN_DPAD_DOWN | BTN_DPAD_LEFT); break;
                case 7: xinputButtons |= BTN_DPAD_LEFT; break;
                case 8: xinputButtons |= (BTN_DPAD_UP | BTN_DPAD_LEFT); break;
                default: break;
            }

            try {
                nativeSetVirtualGamepad(xinputButtons, lx, -ly, rx, -ry, lt, rt);
            } catch (UnsatisfiedLinkError err) {
                Log.e(TAG, "nativeSetVirtualGamepad call failed", err);
            }
        });

        // Watch layout changes to update popup size or visibility
        View decorView = getWindow().getDecorView();
        if (decorView != null) {
            decorView.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                int w = right - left;
                int h = bottom - top;
                if (w > 0 && h > 0) {
                    if (isPhysicalGamepadConnected()) {
                        hideControlsOverlay();
                    } else if (controlsPopup != null && controlsPopup.isShowing()) {
                        controlsPopup.update(0, 0, w, h);
                    } else {
                        showControlsOverlay();
                    }
                }
            });
        }

        // Register InputDeviceListener to detect physical controller connection/disconnection
        inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        inputDeviceListener = new InputManager.InputDeviceListener() {
            @Override
            public void onInputDeviceAdded(int deviceId) {
                Log.i(TAG, "Input device added: id=" + deviceId);
                updateControlsVisibility();
            }

            @Override
            public void onInputDeviceRemoved(int deviceId) {
                Log.i(TAG, "Input device removed: id=" + deviceId);
                updateControlsVisibility();
            }

            @Override
            public void onInputDeviceChanged(int deviceId) {
                Log.i(TAG, "Input device changed: id=" + deviceId);
                updateControlsVisibility();
            }
        };

        if (inputManager != null) {
            inputManager.registerInputDeviceListener(inputDeviceListener, null);
        }

        // ── Engine FPS overlay setup ──────────────────────────────────────────
        setupFpsOverlay();
    }

    /** Creates a minimal on-screen FPS label embedded in the decor view (top-left corner). */
    private void setupFpsOverlay() {
        fpsTextView = new TextView(this);
        fpsTextView.setId(View.generateViewId());

        // Styling: semi-transparent black background, amber monospaced text
        fpsTextView.setBackgroundColor(Color.argb(130, 0, 0, 0));
        fpsTextView.setTextColor(Color.rgb(255, 200, 50));   // amber
        fpsTextView.setTypeface(android.graphics.Typeface.MONOSPACE);
        fpsTextView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f);
        fpsTextView.setPadding(10, 4, 10, 4);
        fpsTextView.setText("-- FPS");
        fpsTextView.setAlpha(0.88f);

        // Anchor to top-left of the decor view; this requires no special permission.
        FrameLayout.LayoutParams flp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT);
        flp.gravity    = Gravity.TOP | Gravity.START;
        flp.leftMargin = 12;
        flp.topMargin  = 12;

        View decor = getWindow().getDecorView();
        if (decor instanceof FrameLayout) {
            ((FrameLayout) decor).addView(fpsTextView, flp);
            startFpsPoller();
        } else {
            Log.w(TAG, "FPS overlay: decor view is not a FrameLayout, skipping");
            fpsTextView = null;
        }
    }

    /** Starts polling nativeGetEngineFps() every FPS_POLL_INTERVAL_MS. */
    private void startFpsPoller() {
        fpsPollHandler  = new Handler(Looper.getMainLooper());
        fpsPollRunnable = new Runnable() {
            @Override
            public void run() {
                updateFpsDisplay();
                if (fpsPollHandler != null) {
                    fpsPollHandler.postDelayed(this, FPS_POLL_INTERVAL_MS);
                }
            }
        };
        fpsPollHandler.postDelayed(fpsPollRunnable, FPS_POLL_INTERVAL_MS);
    }

    /** Reads nativeGetEngineFps() and updates the overlay label. */
    private void updateFpsDisplay() {
        if (fpsTextView == null) return;
        try {
            float fps = nativeGetEngineFps();
            if (fps < 0.5f) {
                fpsTextView.setText("-- FPS");
            } else {
                float ms = (fps > 0) ? 1000.0f / fps : 0.0f;
                fpsTextView.setText(String.format(java.util.Locale.US,
                        "%.1f FPS | %.1f ms", fps, ms));
            }
        } catch (UnsatisfiedLinkError | Exception ignored) {
            // native lib not yet loaded
        }
    }

    /**
     * Checks if a specific InputDevice is a physical gamepad or joystick.
     */
    public static boolean isPhysicalGamepad(InputDevice device) {
        if (device == null || device.isVirtual()) {
            return false;
        }

        int sources = device.getSources();
        boolean isGamepad = (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD;
        boolean isJoystick = (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;

        if (isGamepad || isJoystick) {
            return true;
        }

        // Additional fallback: check for typical gamepad buttons
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            try {
                boolean[] hasKeys = device.hasKeys(new int[]{
                        KeyEvent.KEYCODE_BUTTON_A,
                        KeyEvent.KEYCODE_BUTTON_B,
                        KeyEvent.KEYCODE_BUTTON_X,
                        KeyEvent.KEYCODE_BUTTON_Y
                });
                for (boolean hasKey : hasKeys) {
                    if (hasKey) {
                        return true;
                    }
                }
            } catch (Exception ignored) {
            }
        }

        return false;
    }

    /**
     * Checks if at least one physical gamepad is currently connected to the device.
     */
    public boolean isPhysicalGamepadConnected() {
        int[] deviceIds = InputDevice.getDeviceIds();
        for (int deviceId : deviceIds) {
            InputDevice device = InputDevice.getDevice(deviceId);
            if (isPhysicalGamepad(device)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Updates visibility of the virtual controls overlay based on physical gamepad presence.
     */
    private void updateControlsVisibility() {
        runOnUiThread(() -> {
            boolean hasGamepad = isPhysicalGamepadConnected();
            Log.i(TAG, "updateControlsVisibility: physical gamepad connected = " + hasGamepad);
            if (hasGamepad) {
                hideControlsOverlay();
            } else {
                showControlsOverlay();
            }
        });
    }

    /**
     * Hides the virtual on-screen controls overlay and clears any pending virtual input states.
     */
    private void hideControlsOverlay() {
        runOnUiThread(() -> {
            if (controlsView != null) {
                controlsView.setVisibility(View.GONE);
            }

            if (controlsPopup != null && controlsPopup.isShowing()) {
                try {
                    Log.i(TAG, "Dismissing virtual controls PopupWindow (physical controller active)");
                    controlsPopup.dismiss();
                } catch (Exception e) {
                    Log.e(TAG, "Failed to dismiss controls PopupWindow", e);
                }
            }

            // Reset virtual controller inputs so no keys remain stuck
            try {
                nativeSetVirtualGamepad(0, 0f, 0f, 0f, 0f, 0f, 0f);
            } catch (UnsatisfiedLinkError | Exception ignored) {}
        });
    }

    /**
     * Shows the virtual on-screen controls overlay if no physical controller is connected.
     */
    private void showControlsOverlay() {
        if (isFinishing() || (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1 && isDestroyed())) {
            return;
        }

        if (isPhysicalGamepadConnected()) {
            hideControlsOverlay();
            return;
        }

        View decorView = getWindow().getDecorView();
        if (decorView == null || decorView.getWindowToken() == null) {
            return;
        }

        int width = decorView.getWidth();
        int height = decorView.getHeight();
        if (width <= 0 || height <= 0) {
            decorView.post(this::showControlsOverlay);
            return;
        }

        if (controlsView != null) {
            controlsView.setVisibility(View.VISIBLE);
        }

        if (controlsPopup != null && !controlsPopup.isShowing()) {
            try {
                Log.i(TAG, "Displaying Windroid-emu VirtualControllerInputView overlay (" + width + "x" + height + ")");
                controlsPopup.setWidth(width);
                controlsPopup.setHeight(height);
                controlsPopup.showAtLocation(decorView, Gravity.TOP | Gravity.START, 0, 0);
                return;
            } catch (Exception e) {
                Log.w(TAG, "Failed to reshow existing controlsPopup, recreating", e);
                controlsPopup = null;
            }
        }

        if (controlsPopup == null) {
            controlsPopup = new PopupWindow(controlsView, width, height, false);
            controlsPopup.setTouchable(true);
            controlsPopup.setFocusable(false);
            controlsPopup.setOutsideTouchable(false);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                controlsPopup.setSplitTouchEnabled(true);
            }
            controlsPopup.setBackgroundDrawable(null);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                controlsPopup.setIsClippedToScreen(false);
            }
            try {
                Log.i(TAG, "Displaying Windroid-emu VirtualControllerInputView overlay (" + width + "x" + height + ")");
                controlsPopup.showAtLocation(decorView, Gravity.TOP | Gravity.START, 0, 0);
            } catch (Exception e) {
                Log.e(TAG, "Failed to show controls PopupWindow", e);
            }
        }
    }

    private boolean isGamepadEvent(InputEvent event) {
        InputDevice dev = event.getDevice();
        if (isPhysicalGamepad(dev)) {
            return true;
        }
        int sources = event.getSource();
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (isGamepadEvent(event)) {
            if (controlsPopup != null && controlsPopup.isShowing()) {
                hideControlsOverlay();
            }
        }
        return super.dispatchGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (isGamepadEvent(event)) {
            if (controlsPopup != null && controlsPopup.isShowing()) {
                hideControlsOverlay();
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveMode();
        View decorView = getWindow().getDecorView();
        if (decorView != null) {
            decorView.post(this::updateControlsVisibility);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveMode();
            View decorView = getWindow().getDecorView();
            if (decorView != null) {
                decorView.post(this::updateControlsVisibility);
            }
        }
    }

    @Override
    protected void onDestroy() {
        // Stop the FPS poller first.
        if (fpsPollHandler != null) {
            fpsPollHandler.removeCallbacks(fpsPollRunnable);
            fpsPollHandler = null;
        }
        // Detach the FPS label from the decor view.
        if (fpsTextView != null) {
            try {
                View parent = (View) fpsTextView.getParent();
                if (parent instanceof FrameLayout) {
                    ((FrameLayout) parent).removeView(fpsTextView);
                }
            } catch (Exception ignored) {}
            fpsTextView = null;
        }
        if (inputManager != null && inputDeviceListener != null) {
            try {
                inputManager.unregisterInputDeviceListener(inputDeviceListener);
            } catch (Exception ignored) {}
            inputDeviceListener = null;
        }
        if (controlsPopup != null) {
            try {
                controlsPopup.dismiss();
            } catch (Exception ignored) {}
            controlsPopup = null;
        }
        super.onDestroy();
    }

    private void configureFullscreen() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
    }

    private void applyImmersiveMode() {
        View decorView = getWindow().getDecorView();
        if (decorView != null) {
            int uiOptions = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            decorView.setSystemUiVisibility(uiOptions);
        }
    }
}

