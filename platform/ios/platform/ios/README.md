# MapLibre Native for iOS

[![GitHub Action build status](https://github.com/maplibre/maplibre-native/workflows/ios-ci/badge.svg)](https://github.com/maplibre/maplibre-native/actions/workflows/ios-ci.yml) [![GitHub Action build status](https://github.com/maplibre/maplibre-native/workflows/ios-release/badge.svg)](https://github.com/maplibre/maplibre-native/actions/workflows/ios-release.yml)

A library based on [MapLibre Native](https://github.com/maplibre/maplibre-native) for embedding interactive map views with scalable, customizable vector maps into iOS Applications.

## Getting Started

MapLibre Native for iOS is distributed using the [Swift Package Index](https://swiftpackageindex.com/maplibre/maplibre-gl-native-distribution). To add it to your project, follow the steps below.

1. To add a package dependency to your Xcode project, select File > Swift Packages > Add Package Dependency and enter its repository URL. You can also navigate to your target’s General pane, and in the “Frameworks, Libraries, and Embedded Content” section, click the + button, select Add Other, and choose Add Package Dependency.

2. Either add MapLibre GitHub distribution URL `https://github.com/maplibre/maplibre-gl-native-distribution` or search for `maplibre-native` package.

3. Choose "Next". Xcode should clone the distribution repository and download the binaries.

There is a an open bounty to extend this Getting Started guide ([#809](https://github.com/maplibre/maplibre-native/issues/809)). In the meantime, refer to one of these external guides:

- [Get Started with MapLibre Native for iOS using SwiftUI](https://docs.maptiler.com/maplibre-gl-native-ios/ios-swiftui-basic-get-started/)
- [Get Started With MapLibre Native for iOS using UIKit](https://docs.maptiler.com/maplibre-gl-native-ios/ios-uikit-basic-get-started/)

## Developing

To get started, check out the instructions for building with [bazel](../../BAZEL.md).

## Documentation

- [MapLibre Native for iOS API Reference](https://maplibre.org/maplibre-native/ios/api/)
