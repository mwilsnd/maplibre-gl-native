# Building for iOS
The iOS SDK is built using bazel. To get up and running quickly, follow these steps:

1. Get bazelisk, `brew install bazelisk`
2. Open a terminal and navigate to `platform/ios/platform/ios/scrripts`
3. Run bazel!

## Building the SDK xcframework
`./bazel-package.sh --[static|dynamic] --[release|debug] --flavor [legacy|drawable|split] --teamid ProvisioningTeamID --profile-uuid ProvisioningProfileID`

All compiled frameworks will end up in the `bazel-bin/platform/ios/` path from the root of the repo.

## Generating an Xcode project, running the demo app
Run:
`./bazel-xcodeproj.sh --[release|debug] --flavor [legacy|drawable|split] --teamid ProvisioningTeamID --profile-uuid ProvisioningProfileID [--apikey Maptiler_API_Key]`

`MapLibre.xcodeproj` will be created in `platform/ios`.

## Building for a physical device
To run on a physical device, you'll need a mobile provisioning profile. Follow these steps:
1. Open Xcode and ensure you are signed in with your Apple ID.
2. Open settings, select the accounts tab.
3. Click 'Download Manual Profiles'.
4. Locate your provisioning profiles in `~/Library/MobileDevice/Provisioning\ Profiles`.
5. Select a mobileprovision file and press space to open quick view.
6. Note the UUID and team ID numbers, provide these to the project generation script (`--profile-uuid / --teamid`).

## Troubleshooting
If you have issues running bazel, the first thing you should try is cleaning the project. Run `bazel run //platform/ios:xcodeproj -- clean` and try again.

# Developing
`platform/ios/platform/ios/vendor/BUILD.bazel`
- Covering the iOS specific dependencies.

`BUILD.bazel`
- Covering the base cpp in the root `src` directory.

`vendor/BUILD.bazel`
- Covering the submodule dependencies of Maplibre.

`platform/default/BUILD.bazel`
- Covering the cpp dependencies in default.

`platform/darwin/BUILD.bazel`
- Covering the cpp source in platform/default.

`platform/ios/BUILD.bazel`
- Covering the source in `platform/ios/platform/ios/src` and `platform/ios/platform/darwin/src` as well as defining all the other BUILD.bazel files and defining the xcframework targets.

`/bazel/core.bzl`
- File listings for `mbgl-core`.

`/bazel/flags.bzl`
- Defines some compilation flags that are used between the different build files.

`WORKSPACE`
- Defines the "repo" and the different modules that are loaded in order to compile for Apple.

`.bazelversion`
- Defines the version of bazel used, important for specific support for Apple targets.