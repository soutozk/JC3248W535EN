package com.cyberdeck.spotifybridge;

import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

import java.util.List;

public class SpotifyNotificationListener extends NotificationListenerService {
    private static final String SPOTIFY_PACKAGE = "com.spotify.music";
    private static SpotifyNotificationListener activeInstance;

    @Override
    public void onListenerConnected() {
        activeInstance = this;
        registerSpotifySessionCallback();
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
            pushFromMediaSession(context);
        }
    }

    private void pushCurrentSpotify() {
        if (pushFromMediaSession(this)) {
            return;
        }

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
        if (cover == null) {
            cover = findSpotifyCoverFromMediaSession(context);
        }
        BleBridge.get(context).sendSpotifyTrack(title, artist, cover);
    }

    public static void handleMediaCommand(Context context, int command) {
        MediaController controller = findSpotifyController(context);
        if (controller == null) {
            return;
        }

        MediaController.TransportControls controls = controller.getTransportControls();
        if (command == 1) {
            controls.skipToNext();
        } else if (command == 2) {
            controls.skipToPrevious();
        } else if (command == 3) {
            PlaybackState state = controller.getPlaybackState();
            if (state != null && state.getState() == PlaybackState.STATE_PLAYING) {
                controls.pause();
            } else {
                controls.play();
            }
        }
    }

    private static boolean pushFromMediaSession(Context context) {
        MediaController controller = findSpotifyController(context);
        if (controller == null || controller.getMetadata() == null) {
            return false;
        }

        MediaMetadata metadata = controller.getMetadata();
        String title = metadata.getString(MediaMetadata.METADATA_KEY_TITLE);
        String artist = metadata.getString(MediaMetadata.METADATA_KEY_ARTIST);
        if (artist == null || artist.trim().isEmpty()) {
            artist = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST);
        }
        if (title == null || title.trim().isEmpty()) {
            return false;
        }

        Bitmap cover = coverFromMetadata(metadata);
        BleBridge.get(context).sendSpotifyTrack(title, artist, cover);
        return true;
    }

    private static MediaController findSpotifyController(Context context) {
        MediaSessionManager manager =
                (MediaSessionManager) context.getSystemService(Context.MEDIA_SESSION_SERVICE);
        if (manager == null) {
            return null;
        }

        ComponentName componentName = new ComponentName(context, SpotifyNotificationListener.class);
        List<MediaController> controllers;
        try {
            controllers = manager.getActiveSessions(componentName);
        } catch (SecurityException ex) {
            return null;
        }

        if (controllers == null) {
            return null;
        }
        for (MediaController controller : controllers) {
            if (SPOTIFY_PACKAGE.equals(controller.getPackageName())) {
                return controller;
            }
        }
        return null;
    }

    private static Bitmap findSpotifyCoverFromMediaSession(Context context) {
        MediaController controller = findSpotifyController(context);
        if (controller == null || controller.getMetadata() == null) {
            return null;
        }
        return coverFromMetadata(controller.getMetadata());
    }

    private static Bitmap coverFromMetadata(MediaMetadata metadata) {
        Bitmap bitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
        if (bitmap != null) {
            return bitmap;
        }

        bitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ART);
        if (bitmap != null) {
            return bitmap;
        }

        return metadata.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON);
    }

    private void registerSpotifySessionCallback() {
        MediaSessionManager manager =
                (MediaSessionManager) getSystemService(Context.MEDIA_SESSION_SERVICE);
        if (manager == null) {
            return;
        }

        ComponentName componentName = new ComponentName(this, SpotifyNotificationListener.class);
        try {
            manager.addOnActiveSessionsChangedListener(controllers -> pushCurrentSpotify(), componentName);
        } catch (SecurityException ignored) {
            // The user still needs to enable notification access.
        }
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
