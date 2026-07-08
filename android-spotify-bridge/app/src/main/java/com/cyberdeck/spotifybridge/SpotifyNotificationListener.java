package com.cyberdeck.spotifybridge;

import android.app.Notification;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

public class SpotifyNotificationListener extends NotificationListenerService {
    private static final String SPOTIFY_PACKAGE = "com.spotify.music";
    private static SpotifyNotificationListener activeInstance;

    @Override
    public void onListenerConnected() {
        activeInstance = this;
        pushCurrentSpotify();
    }

    @Override
    public void onListenerDisconnected() {
        if (activeInstance == this) {
            activeInstance = null;
        }
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        if (SPOTIFY_PACKAGE.equals(sbn.getPackageName())) {
            sendNotificationToBridge(this, sbn);
        }
    }

    public static void pushActiveSpotify(Context context) {
        if (activeInstance != null) {
            activeInstance.pushCurrentSpotify();
        } else {
            BleBridge.get(context).sendSpotifyTrack("", "", null);
        }
    }

    private void pushCurrentSpotify() {
        StatusBarNotification[] notifications = getActiveNotifications();
        if (notifications == null) {
            return;
        }
        for (StatusBarNotification sbn : notifications) {
            if (SPOTIFY_PACKAGE.equals(sbn.getPackageName())) {
                sendNotificationToBridge(this, sbn);
                return;
            }
        }
    }

    private static void sendNotificationToBridge(Context context, StatusBarNotification sbn) {
        Notification notification = sbn.getNotification();
        if (notification == null || notification.extras == null) {
            return;
        }

        Bundle extras = notification.extras;
        String title = getText(extras, Notification.EXTRA_TITLE);
        String artist = getText(extras, Notification.EXTRA_TEXT);
        if (title.isEmpty() || "Spotify".equalsIgnoreCase(title)) {
            return;
        }

        Bitmap cover = extractCover(context, notification, extras);
        BleBridge.get(context).sendSpotifyTrack(title, artist, cover);
    }

    private static String getText(Bundle extras, String key) {
        CharSequence value = extras.getCharSequence(key);
        return value == null ? "" : value.toString().trim();
    }

    private static Bitmap extractCover(Context context, Notification notification, Bundle extras) {
        Bitmap bitmap = bitmapFromExtra(context, extras.get(Notification.EXTRA_LARGE_ICON_BIG));
        if (bitmap != null) {
            return bitmap;
        }

        bitmap = bitmapFromExtra(context, extras.get(Notification.EXTRA_LARGE_ICON));
        if (bitmap != null) {
            return bitmap;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && notification.getLargeIcon() != null) {
            Drawable drawable = notification.getLargeIcon().loadDrawable(context);
            return drawableToBitmap(drawable);
        }
        return null;
    }

    private static Bitmap bitmapFromExtra(Context context, Object value) {
        if (value instanceof Bitmap) {
            return (Bitmap) value;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && value instanceof Icon) {
            Drawable drawable = ((Icon) value).loadDrawable(context);
            return drawableToBitmap(drawable);
        }
        if (value instanceof Drawable) {
            return drawableToBitmap((Drawable) value);
        }
        return null;
    }

    private static Bitmap drawableToBitmap(Drawable drawable) {
        if (drawable == null) {
            return null;
        }
        if (drawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) drawable).getBitmap();
        }

        int width = Math.max(1, drawable.getIntrinsicWidth());
        int height = Math.max(1, drawable.getIntrinsicHeight());
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
