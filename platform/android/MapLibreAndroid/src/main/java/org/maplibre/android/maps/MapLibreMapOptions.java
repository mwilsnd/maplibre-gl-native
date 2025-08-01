package org.maplibre.android.maps;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;

import androidx.annotation.ColorInt;
import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.maplibre.android.R;
import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.constants.MapLibreConstants;
import org.maplibre.android.utils.BitmapUtils;
import org.maplibre.android.utils.FontUtils;

import java.util.Arrays;

/**
 * Defines configuration MapLibreMapOptions for a MapLibreMap. These options can be used when adding a
 * map to your application programmatically (as opposed to via XML). If you are using a MapFragment,
 * you can pass these options in using the static factory method newInstance(MapLibreMapOptions).
 * If you are using a MapView, you can pass these options in using the constructor
 * MapView(Context, MapLibreMapOptions). If you add a map using XML, then you can apply these options
 * using custom XML tags.
 */
public class MapLibreMapOptions implements Parcelable {

  private static final int LIGHT_GRAY = 0xFFF0E9E1; // RGB(240, 233, 225))
  private static final float FOUR_DP = 4f;
  private static final float NINETY_TWO_DP = 92f;
  private static final int UNDEFINED_COLOR = -1;

  private CameraPosition cameraPosition;

  private boolean debugActive;

  private boolean compassEnabled = true;
  private boolean fadeCompassFacingNorth = true;
  private int compassGravity = Gravity.TOP | Gravity.END;
  private int[] compassMargins;
  private Drawable compassImage;

  private boolean logoEnabled = true;
  private int logoGravity = Gravity.BOTTOM | Gravity.START;
  private int[] logoMargins;

  @ColorInt
  private int attributionTintColor = UNDEFINED_COLOR;
  private boolean attributionEnabled = true;
  private int attributionGravity = Gravity.BOTTOM | Gravity.START;
  private int[] attributionMargins;

  private double minZoom = MapLibreConstants.MINIMUM_ZOOM;
  private double maxZoom = MapLibreConstants.MAXIMUM_ZOOM;
  private double minPitch = MapLibreConstants.MINIMUM_PITCH;
  private double maxPitch = MapLibreConstants.MAXIMUM_PITCH;

  private boolean rotateGesturesEnabled = true;
  private boolean scrollGesturesEnabled = true;
  private boolean horizontalScrollGesturesEnabled = true;
  private boolean tiltGesturesEnabled = true;
  private boolean zoomGesturesEnabled = true;
  private boolean doubleTapGesturesEnabled = true;
  private boolean quickZoomGesturesEnabled = true;

  private boolean prefetchesTiles = true;
  private int prefetchZoomDelta = 4;
  private boolean zMediaOverlay = false;

  private boolean localIdeographFontFamilyEnabled = true;
  private String localIdeographFontFamily;
  private String[] localIdeographFontFamilies;

  private String apiBaseUri;

  private boolean textureMode;
  private boolean translucentTextureSurface;

  @ColorInt
  private int foregroundLoadColor;

  private float pixelRatio;

  private boolean crossSourceCollisions = true;

  private boolean actionJournalEnabled = false;
  private String actionJournalPath = "";
  private long actionJournalLogFileSize = 1024 * 1024;
  private long actionJournalLogFileCount = 5;
  private int actionJournalRenderingReportInterval = 60;

  /**
   * Creates a new MapLibreMapOptions object.
   *
   * @deprecated Use {@link #createFromAttributes(Context, AttributeSet)} instead.
   */
  @Deprecated
  public MapLibreMapOptions() {
  }

  private MapLibreMapOptions(Parcel in) {
    cameraPosition = in.readParcelable(CameraPosition.class.getClassLoader());
    debugActive = in.readByte() != 0;

    compassEnabled = in.readByte() != 0;
    compassGravity = in.readInt();
    compassMargins = in.createIntArray();
    fadeCompassFacingNorth = in.readByte() != 0;

    Bitmap compassBitmap = in.readParcelable(getClass().getClassLoader());
    if (compassBitmap != null) {
      compassImage = new BitmapDrawable(compassBitmap);
    }

    logoEnabled = in.readByte() != 0;
    logoGravity = in.readInt();
    logoMargins = in.createIntArray();

    attributionEnabled = in.readByte() != 0;
    attributionGravity = in.readInt();
    attributionMargins = in.createIntArray();
    attributionTintColor = in.readInt();

    minZoom = in.readDouble();
    maxZoom = in.readDouble();
    minPitch = in.readDouble();
    maxPitch = in.readDouble();

    rotateGesturesEnabled = in.readByte() != 0;
    scrollGesturesEnabled = in.readByte() != 0;
    horizontalScrollGesturesEnabled = in.readByte() != 0;
    tiltGesturesEnabled = in.readByte() != 0;
    zoomGesturesEnabled = in.readByte() != 0;
    doubleTapGesturesEnabled = in.readByte() != 0;
    quickZoomGesturesEnabled = in.readByte() != 0;

    apiBaseUri = in.readString();
    textureMode = in.readByte() != 0;
    translucentTextureSurface = in.readByte() != 0;
    prefetchesTiles = in.readByte() != 0;
    prefetchZoomDelta = in.readInt();
    zMediaOverlay = in.readByte() != 0;
    localIdeographFontFamilyEnabled = in.readByte() != 0;
    localIdeographFontFamily = in.readString();
    localIdeographFontFamilies = in.createStringArray();
    pixelRatio = in.readFloat();
    foregroundLoadColor = in.readInt();
    crossSourceCollisions = in.readByte() != 0;

    actionJournalEnabled = in.readByte() != 0;
    actionJournalPath = in.readString();
    actionJournalLogFileSize = in.readLong();
    actionJournalLogFileCount = in.readLong();
    actionJournalRenderingReportInterval = in.readInt();
  }

  /**
   * Creates a default MapLibreMapsOptions from a given context.
   *
   * @param context Context related to a map view.
   * @return the MapLibreMapOptions created from attributes
   */
  @NonNull
  public static MapLibreMapOptions createFromAttributes(@NonNull Context context) {
    return createFromAttributes(context, null);
  }

  /**
   * Creates a MapLibreMapsOptions from the attribute set.
   *
   * @param context Context related to a map view.
   * @param attrs   Attributeset containing configuration
   * @return the MapLibreMapOptions created from attributes
   */
  @NonNull
  public static MapLibreMapOptions createFromAttributes(@NonNull Context context, @Nullable AttributeSet attrs) {
    TypedArray typedArray = context.obtainStyledAttributes(attrs, R.styleable.maplibre_MapView, 0, 0);
    return createFromAttributes(new MapLibreMapOptions(), context, typedArray);
  }

  @VisibleForTesting
  static MapLibreMapOptions createFromAttributes(@NonNull MapLibreMapOptions maplibreMapOptions,
                                                 @NonNull Context context,
                                                 @Nullable TypedArray typedArray) {
    float pxlRatio = context.getResources().getDisplayMetrics().density;
    maplibreMapOptions.actionJournalPath(context.getFilesDir().getAbsolutePath());

    try {
      maplibreMapOptions.camera(new CameraPosition.Builder(typedArray).build());

      // deprecated
      maplibreMapOptions.apiBaseUrl(typedArray.getString(R.styleable.maplibre_MapView_maplibre_apiBaseUrl));

      String baseUri = typedArray.getString(R.styleable.maplibre_MapView_maplibre_apiBaseUri);
      if (!TextUtils.isEmpty(baseUri)) {
        // override deprecated property if a value of the new type was provided
        maplibreMapOptions.apiBaseUri(baseUri);
      }

      maplibreMapOptions.zoomGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiZoomGestures, true));
      maplibreMapOptions.scrollGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiScrollGestures, true));
      maplibreMapOptions.horizontalScrollGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiHorizontalScrollGestures, true));
      maplibreMapOptions.rotateGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiRotateGestures, true));
      maplibreMapOptions.tiltGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiTiltGestures, true));
      maplibreMapOptions.doubleTapGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiDoubleTapGestures, true));
      maplibreMapOptions.quickZoomGesturesEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiQuickZoomGestures, true));

      maplibreMapOptions.maxZoomPreference(typedArray.getFloat(R.styleable.maplibre_MapView_maplibre_cameraZoomMax,
        MapLibreConstants.MAXIMUM_ZOOM));
      maplibreMapOptions.minZoomPreference(typedArray.getFloat(R.styleable.maplibre_MapView_maplibre_cameraZoomMin,
        MapLibreConstants.MINIMUM_ZOOM));
      maplibreMapOptions.maxPitchPreference(typedArray.getFloat(R.styleable.maplibre_MapView_maplibre_cameraPitchMax,
        MapLibreConstants.MAXIMUM_PITCH));
      maplibreMapOptions.minPitchPreference(typedArray.getFloat(R.styleable.maplibre_MapView_maplibre_cameraPitchMin,
        MapLibreConstants.MINIMUM_PITCH));

      maplibreMapOptions.compassEnabled(typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiCompass, true));
      maplibreMapOptions.compassGravity(typedArray.getInt(R.styleable.maplibre_MapView_maplibre_uiCompassGravity,
        Gravity.TOP | Gravity.END));
      maplibreMapOptions.compassMargins(new int[] {
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiCompassMarginLeft,
          FOUR_DP * pxlRatio)),
        ((int) typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiCompassMarginTop,
          FOUR_DP * pxlRatio)),
        ((int) typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiCompassMarginRight,
          FOUR_DP * pxlRatio)),
        ((int) typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiCompassMarginBottom,
          FOUR_DP * pxlRatio))});
      maplibreMapOptions.compassFadesWhenFacingNorth(typedArray.getBoolean(
        R.styleable.maplibre_MapView_maplibre_uiCompassFadeFacingNorth, true));
      Drawable compassDrawable = typedArray.getDrawable(
        R.styleable.maplibre_MapView_maplibre_uiCompassDrawable);
      if (compassDrawable == null) {
        compassDrawable = ResourcesCompat.getDrawable(context.getResources(), R.drawable.maplibre_compass_icon, null);
      }
      maplibreMapOptions.compassImage(compassDrawable);

      maplibreMapOptions.logoEnabled(typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_uiLogo, true));
      maplibreMapOptions.logoGravity(typedArray.getInt(R.styleable.maplibre_MapView_maplibre_uiLogoGravity,
        Gravity.BOTTOM | Gravity.START));
      maplibreMapOptions.logoMargins(new int[] {
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiLogoMarginLeft,
          FOUR_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiLogoMarginTop,
          FOUR_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiLogoMarginRight,
          FOUR_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiLogoMarginBottom,
          FOUR_DP * pxlRatio))});

      maplibreMapOptions.attributionTintColor(typedArray.getColor(
        R.styleable.maplibre_MapView_maplibre_uiAttributionTintColor, UNDEFINED_COLOR));
      maplibreMapOptions.attributionEnabled(typedArray.getBoolean(
        R.styleable.maplibre_MapView_maplibre_uiAttribution, true));
      maplibreMapOptions.attributionGravity(typedArray.getInt(
        R.styleable.maplibre_MapView_maplibre_uiAttributionGravity, Gravity.BOTTOM | Gravity.START));
      maplibreMapOptions.attributionMargins(new int[] {
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiAttributionMarginLeft,
          NINETY_TWO_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiAttributionMarginTop,
          FOUR_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiAttributionMarginRight,
          FOUR_DP * pxlRatio)),
        (int) (typedArray.getDimension(R.styleable.maplibre_MapView_maplibre_uiAttributionMarginBottom,
          FOUR_DP * pxlRatio))});
      maplibreMapOptions.textureMode(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_renderTextureMode, false));
      maplibreMapOptions.translucentTextureSurface(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_renderTextureTranslucentSurface, false));
      maplibreMapOptions.setPrefetchesTiles(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_enableTilePrefetch, true));
      maplibreMapOptions.setPrefetchZoomDelta(
        typedArray.getInt(R.styleable.maplibre_MapView_maplibre_prefetchZoomDelta, 4));
      maplibreMapOptions.renderSurfaceOnTop(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_enableZMediaOverlay, false));

      maplibreMapOptions.localIdeographFontFamilyEnabled =
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_localIdeographEnabled, true);

      int localIdeographFontFamiliesResId =
        typedArray.getResourceId(R.styleable.maplibre_MapView_maplibre_localIdeographFontFamilies, 0);
      if (localIdeographFontFamiliesResId != 0) {
        String[] localIdeographFontFamilies =
          context.getResources().getStringArray(localIdeographFontFamiliesResId);
        maplibreMapOptions.localIdeographFontFamily(localIdeographFontFamilies);
      } else {
        // did user provide xml font string?
        String localIdeographFontFamily =
          typedArray.getString(R.styleable.maplibre_MapView_maplibre_localIdeographFontFamily);
        if (localIdeographFontFamily == null) {
          localIdeographFontFamily = MapLibreConstants.DEFAULT_FONT;
        }
        maplibreMapOptions.localIdeographFontFamily(localIdeographFontFamily);
      }

      maplibreMapOptions.pixelRatio(
        typedArray.getFloat(R.styleable.maplibre_MapView_maplibre_pixelRatio, 0));
      maplibreMapOptions.foregroundLoadColor(
        typedArray.getInt(R.styleable.maplibre_MapView_maplibre_foregroundLoadColor, LIGHT_GRAY)
      );
      maplibreMapOptions.crossSourceCollisions(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_cross_source_collisions, true)
      );

      maplibreMapOptions.actionJournalEnabled(
        typedArray.getBoolean(R.styleable.maplibre_MapView_maplibre_actionJournalEnabled, false)
      );
      maplibreMapOptions.actionJournalLogFileSize(
        typedArray.getInteger(R.styleable.maplibre_MapView_maplibre_actionJournalLogFileSize, 1024 * 1024)
      );
      maplibreMapOptions.actionJournalLogFileCount(
        typedArray.getInteger(R.styleable.maplibre_MapView_maplibre_actionJournalLogFileCount, 5)
      );
      maplibreMapOptions.actionJournalRenderingReportInterval(
              typedArray.getInteger(R.styleable.maplibre_MapView_maplibre_actionJournalRenderingReportInterval, 60)
      );
    } finally {
      typedArray.recycle();
    }
    return maplibreMapOptions;
  }

  /**
   * Specifies the URL used for API endpoint.
   *
   * @param apiBaseUrl The base of our API endpoint
   * @return This
   * @deprecated use {@link #apiBaseUri} instead
   */
  @Deprecated
  @NonNull
  public MapLibreMapOptions apiBaseUrl(String apiBaseUrl) {
    this.apiBaseUri = apiBaseUrl;
    return this;
  }

  /**
   * Specifies the URI used for API endpoint.
   *
   * @param apiBaseUri The base of our API endpoint
   * @return This
   */
  @NonNull
  public MapLibreMapOptions apiBaseUri(String apiBaseUri) {
    this.apiBaseUri = apiBaseUri;
    return this;
  }

  /**
   * Specifies a the initial camera position for the map view.
   *
   * @param cameraPosition Inital camera position
   * @return This
   */
  @NonNull
  public MapLibreMapOptions camera(CameraPosition cameraPosition) {
    this.cameraPosition = cameraPosition;
    return this;
  }

  /**
   * Specifies the used debug type for a map view.
   *
   * @param enabled True is debug is enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions debugActive(boolean enabled) {
    debugActive = enabled;
    return this;
  }

  /**
   * Specifies the used minimum zoom level for a map view.
   *
   * @param minZoom Zoom level to be used
   * @return This
   */
  @NonNull
  public MapLibreMapOptions minZoomPreference(double minZoom) {
    this.minZoom = minZoom;
    return this;
  }

  /**
   * Specifies the used maximum zoom level for a map view.
   *
   * @param maxZoom Zoom level to be used
   * @return This
   */
  @NonNull
  public MapLibreMapOptions maxZoomPreference(double maxZoom) {
    this.maxZoom = maxZoom;
    return this;
  }


  /**
   * Specifies the used minimum pitch for a map view.
   *
   * @param minPitch Pitch to be used
   * @return This
   */
  @NonNull
  public MapLibreMapOptions minPitchPreference(double minPitch) {
    this.minPitch = minPitch;
    return this;
  }

  /**
   * Specifies the used maximum pitch for a map view.
   *
   * @param maxPitch Pitch to be used
   * @return This
   */
  @NonNull
  public MapLibreMapOptions maxPitchPreference(double maxPitch) {
    this.maxPitch = maxPitch;
    return this;
  }

  /**
   * Specifies the visibility state of a maplibre_compass_icon for a map view.
   *
   * @param enabled True and maplibre_compass_icon is shown
   * @return This
   */
  @NonNull
  public MapLibreMapOptions compassEnabled(boolean enabled) {
    compassEnabled = enabled;
    return this;
  }

  /**
   * Specifies the gravity state of maplibre_compass_icon for a map view.
   *
   * @param gravity Android SDK Gravity.
   * @return This
   */
  @NonNull
  public MapLibreMapOptions compassGravity(int gravity) {
    compassGravity = gravity;
    return this;
  }

  /**
   * Specifies the margin state of maplibre_compass_icon for a map view
   *
   * @param margins 4 long array for LTRB margins
   * @return This
   */
  @NonNull
  public MapLibreMapOptions compassMargins(int[] margins) {
    compassMargins = margins;
    return this;
  }

  /**
   * Specifies if the maplibre_compass_icon fades to invisible when facing north.
   * <p>
   * By default this value is true.
   * </p>
   *
   * @param compassFadeWhenFacingNorth true is maplibre_compass_icon fades to invisble
   * @return This
   */
  @NonNull
  public MapLibreMapOptions compassFadesWhenFacingNorth(boolean compassFadeWhenFacingNorth) {
    this.fadeCompassFacingNorth = compassFadeWhenFacingNorth;
    return this;
  }

  /**
   * Specifies the image of the CompassView.
   * <p>
   * By default this value is R.drawable.maplibre_compass_icon.
   * </p>
   *
   * @param compass the drawable to show as image compass
   * @return This
   */
  @NonNull
  public MapLibreMapOptions compassImage(Drawable compass) {
    this.compassImage = compass;
    return this;
  }

  /**
   * Specifies the visibility state of a logo for a map view.
   *
   * @param enabled True and logo is shown
   * @return This
   */
  @NonNull
  public MapLibreMapOptions logoEnabled(boolean enabled) {
    logoEnabled = enabled;
    return this;
  }

  /**
   * Specifies the gravity state of logo for a map view.
   *
   * @param gravity Android SDK Gravity.
   * @return This
   */
  @NonNull
  public MapLibreMapOptions logoGravity(int gravity) {
    logoGravity = gravity;
    return this;
  }

  /**
   * Specifies the margin state of logo for a map view
   *
   * @param margins 4 long array for LTRB margins
   * @return This
   */
  @NonNull
  public MapLibreMapOptions logoMargins(int[] margins) {
    logoMargins = margins;
    return this;
  }

  /**
   * Specifies the visibility state of a attribution for a map view.
   *
   * @param enabled True and attribution is shown
   * @return This
   */
  @NonNull
  public MapLibreMapOptions attributionEnabled(boolean enabled) {
    attributionEnabled = enabled;
    return this;
  }

  /**
   * Specifies the gravity state of attribution for a map view.
   *
   * @param gravity Android SDK Gravity.
   * @return This
   */
  @NonNull
  public MapLibreMapOptions attributionGravity(int gravity) {
    attributionGravity = gravity;
    return this;
  }

  /**
   * Specifies the margin state of attribution for a map view
   *
   * @param margins 4 long array for LTRB margins
   * @return This
   */
  @NonNull
  public MapLibreMapOptions attributionMargins(int[] margins) {
    attributionMargins = margins;
    return this;
  }

  /**
   * Specifies the tint color of the attribution for a map view
   *
   * @param color integer resembling a color
   * @return This
   */
  @NonNull
  public MapLibreMapOptions attributionTintColor(@ColorInt int color) {
    attributionTintColor = color;
    return this;
  }

  /**
   * Specifies if the rotate gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions rotateGesturesEnabled(boolean enabled) {
    rotateGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies if the scroll gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions scrollGesturesEnabled(boolean enabled) {
    scrollGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies if the horizontal scroll gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions horizontalScrollGesturesEnabled(boolean enabled) {
    horizontalScrollGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies if the tilt gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions tiltGesturesEnabled(boolean enabled) {
    tiltGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies if the zoom gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions zoomGesturesEnabled(boolean enabled) {
    zoomGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies if the double tap gesture is enabled for a map view.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions doubleTapGesturesEnabled(boolean enabled) {
    doubleTapGesturesEnabled = enabled;
    return this;
  }

  /**
   * Specifies whether the user may zoom the map by tapping twice, holding and moving the pointer up and down.
   *
   * @param enabled True and gesture will be enabled
   * @return This
   */
  @NonNull
  public MapLibreMapOptions quickZoomGesturesEnabled(boolean enabled) {
    quickZoomGesturesEnabled = enabled;
    return this;
  }

  /**
   * Enable {@link android.view.TextureView} as rendered surface.
   * <p>
   * Since the 5.2.0 release we replaced our TextureView with an {@link android.opengl.GLSurfaceView}
   * implementation. Enabling this option will use the {@link android.view.TextureView} instead.
   * {@link android.view.TextureView} can be useful in situations where you need to animate, scale
   * or transform the view. This comes at a siginficant performance penalty and should not be considered
   * unless absolutely needed.
   * </p>
   *
   * @param textureMode True to enable texture mode
   * @return This
   */
  @NonNull
  public MapLibreMapOptions textureMode(boolean textureMode) {
    this.textureMode = textureMode;
    return this;
  }

  @NonNull
  public MapLibreMapOptions translucentTextureSurface(boolean translucentTextureSurface) {
    this.translucentTextureSurface = translucentTextureSurface;
    return this;
  }

  /**
   * Set the MapView foreground color that is used when the map surface is being created.
   *
   * @param loadColor the color to show during map creation
   * @return This
   */
  @NonNull
  public MapLibreMapOptions foregroundLoadColor(@ColorInt int loadColor) {
    this.foregroundLoadColor = loadColor;
    return this;
  }

  /**
   * Enable tile pre-fetching. Loads tiles at a lower zoom-level to pre-render
   * a low resolution preview while more detailed tiles are loaded.
   * Enabled by default
   *
   * @param enable true to enable
   * @return This
   * @deprecated Use {@link #setPrefetchZoomDelta(int)} instead.
   */
  @Deprecated
  @NonNull
  public MapLibreMapOptions setPrefetchesTiles(boolean enable) {
    this.prefetchesTiles = enable;
    return this;
  }

  /**
   * Set the tile pre-fetching zoom delta. Pre-fetching makes sure that a low-resolution
   * tile at the (current_zoom_level - delta) is rendered as soon as possible at the
   * expense of a little bandwidth.
   * Note: This operation will override the MapLibreMapOptions#setPrefetchesTiles(boolean)
   * Setting zoom delta to 0 will disable pre-fetching.
   * Default zoom delta is 4.
   *
   * @param delta zoom delta
   * @return This
   */
  @NonNull
  public MapLibreMapOptions setPrefetchZoomDelta(@IntRange(from = 0) int delta) {
    this.prefetchZoomDelta = delta;
    return this;
  }

  /**
   * Enable cross-source symbol collision detection, defaults to true.
   * <p>
   * If set to false, symbol layers will only run collision detection against
   * other symbol layers that are part of the same source.
   * </p>
   *
   * @param crossSourceCollisions true to enable, false to disable
   * @return This
   */
  @NonNull
  public MapLibreMapOptions crossSourceCollisions(boolean crossSourceCollisions) {
    this.crossSourceCollisions = crossSourceCollisions;
    return this;
  }

  /**
   * Enable action journal event logging, defaults to false.
   * <p>
   * If set to true, enables map event file logging (obtainable via MapView#getActionJournalLog)
   * </p>
   *
   * @param actionJournalEnabled true to enable, false to disable
   * @return This
   */
  @NonNull
  public MapLibreMapOptions actionJournalEnabled(boolean actionJournalEnabled) {
    this.actionJournalEnabled = actionJournalEnabled;
    return this;
  }

  /**
   * Set the action journal log path.
   *
   * @param actionJournalPath Path to be used
   * @return This
   */
  @NonNull
  public MapLibreMapOptions actionJournalPath(@NonNull String actionJournalPath) {
    this.actionJournalPath = actionJournalPath;
    return this;
  }

  /**
   * Set the action journal log file size.
   * <p>
   * The action journal uses a rolling log with multiple files.
   * Total log size is equal to `actionJournalLogFileSize * actionJournalLogFileCount`.
   * </p>
   *
   * @param actionJournalLogFileSize maximum log file size
   * @return This
   */
  @NonNull
  public MapLibreMapOptions actionJournalLogFileSize(long actionJournalLogFileSize) {
    this.actionJournalLogFileSize = actionJournalLogFileSize;
    return this;
  }

  /**
   * Set the action journal log file count.
   * <p>
   * The action journal uses a rolling log with multiple files.
   * Total log size is equal to `actionJournalLogFileSize * actionJournalLogFileCount`.
   * </p>
   *
   * @param actionJournalLogFileCount maximum number of log files
   * @return This
   */
  @NonNull
  public MapLibreMapOptions actionJournalLogFileCount(long actionJournalLogFileCount) {
    this.actionJournalLogFileCount = actionJournalLogFileCount;
    return this;
  }

  /**
   * Set the number of seconds to wait between rendering stats reports.
   *
   * @param actionJournalRenderingReportInterval time interval in seconds
   * @return This
   */
  @NonNull
  public MapLibreMapOptions actionJournalRenderingReportInterval(int actionJournalRenderingReportInterval) {
    this.actionJournalRenderingReportInterval = actionJournalRenderingReportInterval;
    return this;
  }

  /**
   * Enable local ideograph font family, defaults to true.
   *
   * @param enabled true to enable, false to disable
   * @return This
   */
  @NonNull
  public MapLibreMapOptions localIdeographFontFamilyEnabled(boolean enabled) {
    this.localIdeographFontFamilyEnabled = enabled;
    return this;
  }

  /**
   * Set the font family for generating glyphs locally for ideographs in the &#x27;CJK Unified Ideographs&#x27;
   * and &#x27;Hangul Syllables&#x27; ranges.
   * <p>
   * The font family argument is passed to {@link android.graphics.Typeface#create(String, int)}.
   * Default system fonts are defined in &#x27;/system/etc/fonts.xml&#x27;
   * Default font for local ideograph font family is {@link MapLibreConstants#DEFAULT_FONT}.
   *
   * @param fontFamily font family for local ideograph generation.
   * @return This
   */
  @NonNull
  public MapLibreMapOptions localIdeographFontFamily(String fontFamily) {
    this.localIdeographFontFamily = FontUtils.extractValidFont(fontFamily);
    return this;
  }

  /**
   * Set a font family from range of font families for generating glyphs locally for ideographs in the
   * &#x27;CJK Unified Ideographs&#x27; and &#x27;Hangul Syllables&#x27; ranges. The first matching font
   * will be selected. If no valid font found, it defaults to {@link MapLibreConstants#DEFAULT_FONT}.
   * <p>
   * The font families are checked against the default system fonts defined in
   * &#x27;/system/etc/fonts.xml&#x27; Default font for local ideograph font family is
   * {@link MapLibreConstants#DEFAULT_FONT}.
   * </p>
   *
   * @param fontFamilies an array of font families for local ideograph generation.
   * @return This
   */
  @NonNull
  public MapLibreMapOptions localIdeographFontFamily(String... fontFamilies) {
    this.localIdeographFontFamily = FontUtils.extractValidFont(fontFamilies);
    return this;
  }

  /**
   * Set the custom pixel ratio configuration to override the default value from resources.
   * This ratio will be used to initialise the map with.
   *
   * @param pixelRatio the custom pixel ratio of the map under construction
   * @return This
   */
  @NonNull
  public MapLibreMapOptions pixelRatio(float pixelRatio) {
    this.pixelRatio = pixelRatio;
    return this;
  }

  /**
   * Check whether tile pre-fetching is enabled.
   *
   * @return true if enabled
   * @deprecated Use {@link #getPrefetchZoomDelta()} instead.
   */
  @Deprecated
  public boolean getPrefetchesTiles() {
    return prefetchesTiles;
  }

  /**
   * Check current pre-fetching zoom delta.
   *
   * @return current zoom delta.
   */
  @IntRange(from = 0)
  public int getPrefetchZoomDelta() {
    return prefetchZoomDelta;
  }

  /**
   * Check whether cross-source symbol collision detection is enabled.
   *
   * @return true if enabled
   */
  public boolean getCrossSourceCollisions() {
    return crossSourceCollisions;
  }

  /**
   * Check whether action journal logging is enabled.
   *
   * @return true if enabled
   */
  public boolean getActionJournalEnabled() {
    return actionJournalEnabled;
  }

  /**
   * Get the current configured action journal log path.
   *
   * @return log file path
   */
  public String getActionJournalPath() {
    return actionJournalPath;
  }

  /**
   * Get the current configured action journal log file size.
   * Total log size is equal to `actionJournalLogFileSize * actionJournalLogFileCount`.
   *
   * @return maximum file size
   */
  public long getActionJournalLogFileSize() {
    return actionJournalLogFileSize;
  }

  /**
   * Get the current configured action journal log file count.
   * Total log size is equal to `actionJournalLogFileSize * actionJournalLogFileCount`.
   *
   * @return maximum log files used
   */
  public long getActionJournalLogFileCount() {
    return actionJournalLogFileCount;
  }

  /**
   * Get the current configured action journal rendering stats report time interval.
   *
   * @return time interval in seconds
   */
  public int getActionJournalRenderingReportInterval() {
    return actionJournalRenderingReportInterval;
  }

  /**
   * Set the flag to render the map surface on top of another surface.
   *
   * @param renderOnTop true if this map is shown on top of another one, false if bottom.
   */
  public void renderSurfaceOnTop(boolean renderOnTop) {
    this.zMediaOverlay = renderOnTop;
  }

  /**
   * Get the flag to render the map surface on top of another surface.
   *
   * @return true if this map is
   */
  public boolean getRenderSurfaceOnTop() {
    return zMediaOverlay;
  }

  /**
   * Get the current configured API endpoint base URL.
   *
   * @return Base URL to be used API endpoint.
   * @deprecated use {@link #getApiBaseUri()} instead
   */
  @Deprecated
  public String getApiBaseUrl() {
    return apiBaseUri;
  }

  /**
   * Get the current configured API endpoint base URI.
   *
   * @return Base URI to be used API endpoint.
   */
  public String getApiBaseUri() {
    return apiBaseUri;
  }

  /**
   * Get the current configured initial camera position for a map view.
   *
   * @return CameraPosition to be initially used.
   */
  public CameraPosition getCamera() {
    return cameraPosition;
  }

  /**
   * Get the current configured min zoom for a map view.
   *
   * @return Mininum zoom level to be used.
   */
  public double getMinZoomPreference() {
    return minZoom;
  }

  /**
   * Get the current configured maximum zoom for a map view.
   *
   * @return Maximum zoom to be used.
   */
  public double getMaxZoomPreference() {
    return maxZoom;
  }

  /**
   * Get the current configured min pitch for a map view.
   *
   * @return Mininum pitch to be used.
   */
  public double getMinPitchPreference() {
    return minPitch;
  }

  /**
   * Get the current configured maximum pitch for a map view.
   *
   * @return Maximum pitch to be used.
   */
  public double getMaxPitchPreference() {
    return maxPitch;
  }

  /**
   * Get the current configured visibility state for maplibre_compass_icon for a map view.
   *
   * @return Visibility state of the maplibre_compass_icon
   */
  public boolean getCompassEnabled() {
    return compassEnabled;
  }

  /**
   * Get the current configured gravity state for maplibre_compass_icon for a map view.
   *
   * @return Gravity state of the maplibre_compass_icon
   */
  public int getCompassGravity() {
    return compassGravity;
  }

  /**
   * Get the current configured margins for maplibre_compass_icon for a map view.
   *
   * @return Margins state of the maplibre_compass_icon
   */
  public int[] getCompassMargins() {
    return compassMargins;
  }

  /**
   * Get the current configured state for fading the maplibre_compass_icon when facing north.
   *
   * @return True if maplibre_compass_icon fades to invisible when facing north
   */
  public boolean getCompassFadeFacingNorth() {
    return fadeCompassFacingNorth;
  }

  /**
   * Get the current configured CompassView image.
   *
   * @return the drawable used as compass image
   */
  public Drawable getCompassImage() {
    return compassImage;
  }

  /**
   * Get the current configured visibility state for maplibre_compass_icon for a map view.
   *
   * @return Visibility state of the maplibre_compass_icon
   */
  public boolean getLogoEnabled() {
    return logoEnabled;
  }

  /**
   * Get the current configured gravity state for logo for a map view.
   *
   * @return Gravity state of the logo
   */
  public int getLogoGravity() {
    return logoGravity;
  }

  /**
   * Get the current configured margins for logo for a map view.
   *
   * @return Margins state of the logo
   */
  public int[] getLogoMargins() {
    return logoMargins;
  }

  /**
   * Get the current configured rotate gesture state for a map view.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getRotateGesturesEnabled() {
    return rotateGesturesEnabled;
  }

  /**
   * Get the current configured scroll gesture state for a map view.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getScrollGesturesEnabled() {
    return scrollGesturesEnabled;
  }

  /**
   * Get the current configured horizontal scroll gesture state for a map view.
   *
   * @return True indicates horizontal scroll gesture is enabled
   */
  public boolean getHorizontalScrollGesturesEnabled() {
    return horizontalScrollGesturesEnabled;
  }

  /**
   * Get the current configured tilt gesture state for a map view.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getTiltGesturesEnabled() {
    return tiltGesturesEnabled;
  }

  /**
   * Get the current configured zoom gesture state for a map view.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getZoomGesturesEnabled() {
    return zoomGesturesEnabled;
  }

  /**
   * Get the current configured double tap gesture state for a map view.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getDoubleTapGesturesEnabled() {
    return doubleTapGesturesEnabled;
  }

  /**
   * Get whether the user may zoom the map by tapping twice, holding and moving the pointer up and down.
   *
   * @return True indicates gesture is enabled
   */
  public boolean getQuickZoomGesturesEnabled() {
    return quickZoomGesturesEnabled;
  }

  /**
   * Get the current configured visibility state for attribution for a map view.
   *
   * @return Visibility state of the attribution
   */
  public boolean getAttributionEnabled() {
    return attributionEnabled;
  }

  /**
   * Get the current configured gravity state for attribution for a map view.
   *
   * @return Gravity state of the logo
   */
  public int getAttributionGravity() {
    return attributionGravity;
  }

  /**
   * Get the current configured margins for attribution for a map view.
   *
   * @return Margins state of the logo
   */
  public int[] getAttributionMargins() {
    return attributionMargins;
  }

  /**
   * Get the current configured tint color for attribution for a map view.
   *
   * @return the tint color
   */
  @ColorInt
  public int getAttributionTintColor() {
    return attributionTintColor;
  }

  /**
   * Get the current configured debug state for a map view.
   *
   * @return True indicates debug is enabled.
   */
  public boolean getDebugActive() {
    return debugActive;
  }

  /**
   * Returns true if TextureView is being used the render view.
   *
   * @return True if TextureView is used.
   */
  public boolean getTextureMode() {
    return textureMode;
  }

  /**
   * Returns true if TextureView supports a translucent surface
   *
   * @return True if translucent surface is active
   */
  public boolean getTranslucentTextureSurface() {
    return translucentTextureSurface;
  }

  /**
   * Returns the current configured foreground color that is used during map creation.
   *
   * @return the load color
   */
  @ColorInt
  public int getForegroundLoadColor() {
    return foregroundLoadColor;
  }

  /**
   * Returns the font-family for locally overriding generation of glyphs in the
   * &#x27;CJK Unified Ideographs&#x27; and &#x27;Hangul Syllables&#x27; ranges.
   * Default font for local ideograph font family is {@link MapLibreConstants#DEFAULT_FONT}.
   * Returns null if local ideograph font families are disabled.
   *
   * @return Local ideograph font family name.
   */
  @Nullable
  public String getLocalIdeographFontFamily() {
    return localIdeographFontFamilyEnabled ? localIdeographFontFamily : null;
  }

  /**
   * Returns true if local ideograph font family is enabled, defaults to true.
   *
   * @return True if local ideograph font family is enabled
   */
  public boolean isLocalIdeographFontFamilyEnabled() {
    return localIdeographFontFamilyEnabled;
  }

  /**
   * Return the custom configured pixel ratio, returns 0 if not configured.
   *
   * @return the pixel ratio used by the map under construction
   */
  public float getPixelRatio() {
    return pixelRatio;
  }

  public static final Parcelable.Creator<MapLibreMapOptions> CREATOR = new Parcelable.Creator<MapLibreMapOptions>() {
    public MapLibreMapOptions createFromParcel(@NonNull Parcel in) {
      return new MapLibreMapOptions(in);
    }

    public MapLibreMapOptions[] newArray(int size) {
      return new MapLibreMapOptions[size];
    }
  };

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(@NonNull Parcel dest, int flags) {
    dest.writeParcelable(cameraPosition, flags);
    dest.writeByte((byte) (debugActive ? 1 : 0));

    dest.writeByte((byte) (compassEnabled ? 1 : 0));
    dest.writeInt(compassGravity);
    dest.writeIntArray(compassMargins);
    dest.writeByte((byte) (fadeCompassFacingNorth ? 1 : 0));
    dest.writeParcelable(compassImage != null
      ? BitmapUtils.getBitmapFromDrawable(compassImage) : null, flags);

    dest.writeByte((byte) (logoEnabled ? 1 : 0));
    dest.writeInt(logoGravity);
    dest.writeIntArray(logoMargins);

    dest.writeByte((byte) (attributionEnabled ? 1 : 0));
    dest.writeInt(attributionGravity);
    dest.writeIntArray(attributionMargins);
    dest.writeInt(attributionTintColor);

    dest.writeDouble(minZoom);
    dest.writeDouble(maxZoom);
    dest.writeDouble(minPitch);
    dest.writeDouble(maxPitch);

    dest.writeByte((byte) (rotateGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (scrollGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (horizontalScrollGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (tiltGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (zoomGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (doubleTapGesturesEnabled ? 1 : 0));
    dest.writeByte((byte) (quickZoomGesturesEnabled ? 1 : 0));

    dest.writeString(apiBaseUri);
    dest.writeByte((byte) (textureMode ? 1 : 0));
    dest.writeByte((byte) (translucentTextureSurface ? 1 : 0));
    dest.writeByte((byte) (prefetchesTiles ? 1 : 0));
    dest.writeInt(prefetchZoomDelta);
    dest.writeByte((byte) (zMediaOverlay ? 1 : 0));
    dest.writeByte((byte) (localIdeographFontFamilyEnabled ? 1 : 0));
    dest.writeString(localIdeographFontFamily);
    dest.writeStringArray(localIdeographFontFamilies);
    dest.writeFloat(pixelRatio);
    dest.writeInt(foregroundLoadColor);
    dest.writeByte((byte) (crossSourceCollisions ? 1 : 0));

    dest.writeByte((byte) (actionJournalEnabled ? 1 : 0));
    dest.writeString(actionJournalPath);
    dest.writeLong(actionJournalLogFileSize);
    dest.writeLong(actionJournalLogFileCount);
    dest.writeInt(actionJournalRenderingReportInterval);
  }

  @Override
  public boolean equals(@Nullable Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }

    MapLibreMapOptions options = (MapLibreMapOptions) o;

    if (debugActive != options.debugActive) {
      return false;
    }
    if (compassEnabled != options.compassEnabled) {
      return false;
    }
    if (fadeCompassFacingNorth != options.fadeCompassFacingNorth) {
      return false;
    }
    if (compassImage != null
      ? !compassImage.equals(options.compassImage)
      : options.compassImage != null) {
      return false;
    }
    if (compassGravity != options.compassGravity) {
      return false;
    }
    if (logoEnabled != options.logoEnabled) {
      return false;
    }
    if (logoGravity != options.logoGravity) {
      return false;
    }
    if (attributionTintColor != options.attributionTintColor) {
      return false;
    }
    if (attributionEnabled != options.attributionEnabled) {
      return false;
    }
    if (attributionGravity != options.attributionGravity) {
      return false;
    }
    if (Double.compare(options.minZoom, minZoom) != 0) {
      return false;
    }
    if (Double.compare(options.maxZoom, maxZoom) != 0) {
      return false;
    }
    if (Double.compare(options.minPitch, minPitch) != 0) {
      return false;
    }
    if (Double.compare(options.maxPitch, maxPitch) != 0) {
      return false;
    }
    if (rotateGesturesEnabled != options.rotateGesturesEnabled) {
      return false;
    }
    if (scrollGesturesEnabled != options.scrollGesturesEnabled) {
      return false;
    }
    if (horizontalScrollGesturesEnabled != options.horizontalScrollGesturesEnabled) {
      return false;
    }
    if (tiltGesturesEnabled != options.tiltGesturesEnabled) {
      return false;
    }
    if (zoomGesturesEnabled != options.zoomGesturesEnabled) {
      return false;
    }
    if (doubleTapGesturesEnabled != options.doubleTapGesturesEnabled) {
      return false;
    }
    if (quickZoomGesturesEnabled != options.quickZoomGesturesEnabled) {
      return false;
    }
    if (cameraPosition != null ? !cameraPosition.equals(options.cameraPosition) : options.cameraPosition != null) {
      return false;
    }
    if (!Arrays.equals(compassMargins, options.compassMargins)) {
      return false;
    }
    if (!Arrays.equals(logoMargins, options.logoMargins)) {
      return false;
    }
    if (!Arrays.equals(attributionMargins, options.attributionMargins)) {
      return false;
    }
    if (apiBaseUri != null ? !apiBaseUri.equals(options.apiBaseUri) : options.apiBaseUri != null) {
      return false;
    }
    if (prefetchesTiles != options.prefetchesTiles) {
      return false;
    }
    if (prefetchZoomDelta != options.prefetchZoomDelta) {
      return false;
    }
    if (zMediaOverlay != options.zMediaOverlay) {
      return false;
    }
    if (localIdeographFontFamilyEnabled != options.localIdeographFontFamilyEnabled) {
      return false;
    }
    if (!localIdeographFontFamily.equals(options.localIdeographFontFamily)) {
      return false;
    }
    if (!Arrays.equals(localIdeographFontFamilies, options.localIdeographFontFamilies)) {
      return false;
    }

    if (pixelRatio != options.pixelRatio) {
      return false;
    }

    if (crossSourceCollisions != options.crossSourceCollisions) {
      return false;
    }

    if (actionJournalEnabled != options.actionJournalEnabled) {
      return false;
    }

    if (actionJournalPath.equals(options.actionJournalPath)) {
      return false;
    }

    if (actionJournalLogFileSize != options.actionJournalLogFileSize) {
      return false;
    }

    if (actionJournalLogFileCount != options.actionJournalLogFileCount) {
      return false;
    }

    if (actionJournalRenderingReportInterval != options.actionJournalRenderingReportInterval) {
      return false;
    }

    return false;
  }

  @Override
  public int hashCode() {
    int result;
    long temp;
    result = cameraPosition != null ? cameraPosition.hashCode() : 0;
    result = 31 * result + (debugActive ? 1 : 0);
    result = 31 * result + (compassEnabled ? 1 : 0);
    result = 31 * result + (fadeCompassFacingNorth ? 1 : 0);
    result = 31 * result + compassGravity;
    result = 31 * result + (compassImage != null ? compassImage.hashCode() : 0);
    result = 31 * result + Arrays.hashCode(compassMargins);
    result = 31 * result + (logoEnabled ? 1 : 0);
    result = 31 * result + logoGravity;
    result = 31 * result + Arrays.hashCode(logoMargins);
    result = 31 * result + attributionTintColor;
    result = 31 * result + (attributionEnabled ? 1 : 0);
    result = 31 * result + attributionGravity;
    result = 31 * result + Arrays.hashCode(attributionMargins);
    temp = Double.doubleToLongBits(minZoom);
    result = 31 * result + (int) (temp ^ (temp >>> 32));
    temp = Double.doubleToLongBits(maxZoom);
    result = 31 * result + (int) (temp ^ (temp >>> 32));
    temp = Double.doubleToLongBits(minPitch);
    result = 31 * result + (int) (temp ^ (temp >>> 32));
    temp = Double.doubleToLongBits(maxPitch);
    result = 31 * result + (int) (temp ^ (temp >>> 32));
    result = 31 * result + (rotateGesturesEnabled ? 1 : 0);
    result = 31 * result + (scrollGesturesEnabled ? 1 : 0);
    result = 31 * result + (horizontalScrollGesturesEnabled ? 1 : 0);
    result = 31 * result + (tiltGesturesEnabled ? 1 : 0);
    result = 31 * result + (zoomGesturesEnabled ? 1 : 0);
    result = 31 * result + (doubleTapGesturesEnabled ? 1 : 0);
    result = 31 * result + (quickZoomGesturesEnabled ? 1 : 0);
    result = 31 * result + (apiBaseUri != null ? apiBaseUri.hashCode() : 0);
    result = 31 * result + (textureMode ? 1 : 0);
    result = 31 * result + (translucentTextureSurface ? 1 : 0);
    result = 31 * result + (prefetchesTiles ? 1 : 0);
    result = 31 * result + prefetchZoomDelta;
    result = 31 * result + (zMediaOverlay ? 1 : 0);
    result = 31 * result + (localIdeographFontFamilyEnabled ? 1 : 0);
    result = 31 * result + (localIdeographFontFamily != null ? localIdeographFontFamily.hashCode() : 0);
    result = 31 * result + Arrays.hashCode(localIdeographFontFamilies);
    result = 31 * result + (int) pixelRatio;
    result = 31 * result + (crossSourceCollisions ? 1 : 0);
    result = 31 * result + (actionJournalEnabled ? 1 : 0);
    result = 31 * result + (actionJournalPath != null ? actionJournalPath.hashCode() : 0);
    result = 31 * result + (int) actionJournalLogFileSize;
    result = 31 * result + (int) actionJournalLogFileCount;
    result = 31 * result + (int) actionJournalRenderingReportInterval;
    return result;
  }
}
