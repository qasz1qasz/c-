package com.gamemod;

import android.app.Activity;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {

    private NativeLib nativeLib;
    private WindowManager windowManager;
    private View floatingView;
    private WindowManager.LayoutParams floatingParams;
    private Switch switchAntiBan;
    private TextView tvStatus;

    // 拖动悬浮窗时记录的起始位置
    private float dragStartX;
    private float dragStartY;
    private int paramStartX;
    private int paramStartY;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeLib = new NativeLib();

        if (!Settings.canDrawOverlays(this)) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, 1001);
        } else {
            createFloatingWindow();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 1001) {
            if (Settings.canDrawOverlays(this)) {
                createFloatingWindow();
            } else {
                Toast.makeText(this, "需要悬浮窗权限才能运行", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    private void createFloatingWindow() {
        if (floatingView != null) {
            // 已经建过悬浮窗就不重复建，避免叠两个
            moveTaskToBack(true);
            return;
        }

        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        floatingView = View.inflate(this, R.layout.floating_window, null);

        int layoutFlag;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            layoutFlag = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            layoutFlag = WindowManager.LayoutParams.TYPE_PHONE;
        }

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                layoutFlag,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 50;
        params.y = 200;
        floatingParams = params;

        windowManager.addView(floatingView, params);

        tvStatus = floatingView.findViewById(R.id.tvStatus);
        Button btnGetProcess = floatingView.findViewById(R.id.btnGetProcess);
        Button btnModify = floatingView.findViewById(R.id.btnModify);
        Button btnClose = floatingView.findViewById(R.id.btnClose);
        switchAntiBan = floatingView.findViewById(R.id.switchAntiBan);
        TextView tvTitle = floatingView.findViewById(R.id.tvTitle);

        // 按住标题栏拖动悬浮窗
        tvTitle.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    dragStartX = event.getRawX();
                    dragStartY = event.getRawY();
                    paramStartX = floatingParams.x;
                    paramStartY = floatingParams.y;
                    return true;
                case MotionEvent.ACTION_MOVE:
                    floatingParams.x = paramStartX + (int) (event.getRawX() - dragStartX);
                    floatingParams.y = paramStartY + (int) (event.getRawY() - dragStartY);
                    windowManager.updateViewLayout(floatingView, floatingParams);
                    return true;
                default:
                    return false;
            }
        });

        // 获取游戏进程按钮（原生调用放后台线程，避免卡住界面）
        btnGetProcess.setOnClickListener(v -> {
            new Thread(() -> {
                int pid = nativeLib.findGameProcess();
                long base = (pid > 0) ? nativeLib.getModuleBase() : 0;
                runOnUiThread(() -> {
                    if (pid > 0) {
                        tvStatus.setText("进程PID：" + pid + "\n模块基址：0x" + Long.toHexString(base));
                        Toast.makeText(MainActivity.this, "已获取游戏进程", Toast.LENGTH_SHORT).show();
                    } else {
                        tvStatus.setText("未找到游戏进程");
                        Toast.makeText(MainActivity.this, "请先启动游戏", Toast.LENGTH_SHORT).show();
                    }
                });
            }).start();
        });

        // 修改功能按钮（原生调用放后台线程，避免卡住界面）
        btnModify.setOnClickListener(v -> {
            boolean antiBan = switchAntiBan.isChecked();
            new Thread(() -> {
                boolean ok = nativeLib.writeValue(antiBan);
                runOnUiThread(() -> {
                    if (ok) {
                        tvStatus.setText("修改成功！\n新值：110\n地址已写入内存");
                        Toast.makeText(MainActivity.this, "修改成功", Toast.LENGTH_SHORT).show();
                    } else {
                        tvStatus.setText("修改失败\n请检查进程和权限");
                        Toast.makeText(MainActivity.this, "修改失败", Toast.LENGTH_SHORT).show();
                    }
                });
            }).start();
        });

        // 关闭按钮：移除悬浮窗并真正退出
        btnClose.setOnClickListener(v -> {
            removeFloatingWindow();
            finish();
        });

        // 修复原问题：原代码这里调 finish()，Activity 一销毁 onDestroy 就把悬浮窗删了。
        // 改成把任务移后台：Activity 不销毁，悬浮窗能留住。
        moveTaskToBack(true);
    }

    private void removeFloatingWindow() {
        if (windowManager != null && floatingView != null) {
            windowManager.removeView(floatingView);
            floatingView = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        removeFloatingWindow();
    }
}
