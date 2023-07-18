workspace(name = "Maplibre")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "build_bazel_rules_apple",
    sha256 = "8ac4c7997d863f3c4347ba996e831b5ec8f7af885ee8d4fe36f1c3c8f0092b2c",
    url = "https://github.com/bazelbuild/rules_apple/releases/download/2.5.0/rules_apple.2.5.0.tar.gz",
)

http_archive(
    name = "rules_xcodeproj",
    sha256 = "24aed0bc6cf4132f62e08d4b81656107ea8d7804c9f7f57cfb9acbd0992ba75b",
    url = "https://github.com/MobileNativeFoundation/rules_xcodeproj/releases/download/1.8.1/release.tar.gz",
)

load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)

apple_rules_dependencies()

load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)

swift_rules_dependencies()

load(
    "@build_bazel_rules_swift//swift:extras.bzl",
    "swift_rules_extra_dependencies",
)

swift_rules_extra_dependencies()

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

apple_support_dependencies()

load(
    "@rules_xcodeproj//xcodeproj:repositories.bzl",
    "xcodeproj_rules_dependencies",
)

xcodeproj_rules_dependencies()

load(
    "@build_bazel_rules_apple//apple:apple.bzl",
    "provisioning_profile_repository",
)

provisioning_profile_repository(
    name = "local_provisioning_profiles",
)

# Load the Android build rules
http_archive(
    name = "build_bazel_rules_android",
    urls = ["https://github.com/bazelbuild/rules_android/archive/v0.1.1.zip"],
    sha256 = "cd06d15dd8bb59926e4d65f9003bfc20f9da4b2519985c27e190cddc8b7a7806",
    strip_prefix = "rules_android-0.1.1",
)

# Configure Android SDK Path
load("@build_bazel_rules_android//android:rules.bzl", "android_sdk_repository")
android_sdk_repository(
    name = "androidsdk",
)

RULES_JVM_EXTERNAL_TAG = "4.5"
RULES_JVM_EXTERNAL_SHA = "b17d7388feb9bfa7f2fa09031b32707df529f26c91ab9e5d909eb1676badd9a6"
http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = RULES_JVM_EXTERNAL_SHA,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "com.android.tools.build:gradle:8.0.2",

        "org.maplibre.gl:android-sdk-geojson:5.9.0",
        "org.maplibre.gl:android-sdk-turf:5.9.0",
        "com.mapbox.mapboxsdk:mapbox-android-gestures:0.7.0",

        "junit:junit:4.13.2",
        "androidx.test.ext:junit:1.1.4",
        "org.mockito:mockito-core:4.10.0",
        "io.mockk:mockk:1.13.3",
        "org.robolectric:robolectric:4.9.1",
        "org.assertj:assertj-core:3.23.1",
        "androidx.test.ext:junit:1.1.4",
        "androidx.test:rules:1.5.0",
        "androidx.test.espresso:espresso-core:3.5.0",
        "androidx.test.espresso:espresso-intents:3.5.0",
        "androidx.test.espresso:espresso-contrib:3.5.0",
        "androidx.test.uiautomator:uiautomator:2.2.0",
        "androidx.annotation:annotation:1.5.0",
        "androidx.appcompat:appcompat:1.5.1",
        "androidx.fragment:fragment:1.5.5",
        "androidx.lifecycle:lifecycle-runtime:2.5.1",
        "androidx.recyclerview:recyclerview:1.2.1",
        "androidx.constraintlayout:constraintlayout:2.1.4",
        "androidx.interpolator:interpolator:1.0.0",
        # "com.google.android.material:material:1.7.0",
        "androidx.multidex:multidex:2.0.1",
        "com.microsoft.appcenter:espresso-test-extension:1.5",
        "commons-io:commons-io:2.11.0",
        "com.jakewharton.timber:timber:5.0.1",
        "com.squareup.okhttp3:okhttp:4.11.0",
        "com.squareup.leakcanary:leakcanary-android:2.10",

        "org.jetbrains.kotlin:kotlin-stdlib-jdk8:1.7.20",
        "com.google.devtools.ksp:symbol-processing-api:jar:1.8.21-1.0.11",
        "com.google.devtools.ksp:symbol-processing-cmdline:jar:1.8.21-1.0.11",

        "com.android.tools.lint:lint:30.3.1",
        "com.android.tools.lint:lint-api:30.3.1",
        "com.android.tools.lint:lint-checks:30.3.1",
        "com.android.tools.lint:lint-tests:30.3.1",
    ],
    repositories = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
        "https://oss.sonatype.org/content/repositories/snapshots",
    ],
    version_conflict_policy = "pinned",
)

rules_kotlin_version = "1.8"
rules_kotlin_sha = "01293740a16e474669aba5b5a1fe3d368de5832442f164e4fbfc566815a8bc3a"
http_archive(
    name = "io_bazel_rules_kotlin",
    urls = ["https://github.com/bazelbuild/rules_kotlin/releases/download/v%s/rules_kotlin_release.tgz" % rules_kotlin_version],
    sha256 = rules_kotlin_sha,
)

load("@io_bazel_rules_kotlin//kotlin:repositories.bzl", "kotlin_repositories")
kotlin_repositories()

load("@io_bazel_rules_kotlin//kotlin:core.bzl", "kt_register_toolchains")
kt_register_toolchains() # to use the default toolchain, otherwise see toolchains below
