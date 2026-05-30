# Android GPS + MQTT Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an Android app with two Activities: one for manual MQTT control of the ESP32 and one for GPS speed publishing so the ESP32 can decide when to trigger alarms.

**Architecture:** Convert the Android module from the default Compose starter into a Java + XML app. Keep MQTT concerns in a small reusable helper, keep the manual control screen focused on threshold/alarm commands, and keep the GPS screen focused on location permission, speed reading, and periodic MQTT publishing.

**Tech Stack:** Android SDK, Java, XML layouts, Material components, `LocationManager`, Eclipse Paho MQTT client.

---

### Task 1: Convert the starter app to Java/XML

**Files:**
- Modify: `Android/app/build.gradle.kts`
- Modify: `Android/app/src/main/AndroidManifest.xml`
- Delete: `Android/app/src/main/java/com/example/volantemeteoro/MainActivity.kt`
- Delete: `Android/app/src/main/java/com/example/volantemeteoro/ui/theme/Color.kt`
- Delete: `Android/app/src/main/java/com/example/volantemeteoro/ui/theme/Theme.kt`
- Delete: `Android/app/src/main/java/com/example/volantemeteoro/ui/theme/Type.kt`

- [ ] **Step 1: Remove Compose-only setup and add view-based dependencies**
- [ ] **Step 2: Replace the launcher activity entry with the Java activities**
- [ ] **Step 3: Remove the starter Compose files**
- [ ] **Step 4: Confirm the manifest still launches the app**

### Task 2: Add shared MQTT support

**Files:**
- Create: `Android/app/src/main/java/com/example/volantemeteoro/mqtt/MqttConfig.java`
- Create: `Android/app/src/main/java/com/example/volantemeteoro/mqtt/MqttManager.java`
- Modify: `Android/app/build.gradle.kts`

- [ ] **Step 1: Define the broker, client id, and topic names**
- [ ] **Step 2: Implement connect/publish/subscribe helpers**
- [ ] **Step 3: Add the MQTT dependency**
- [ ] **Step 4: Ensure the helper can be reused from both Activities**

### Task 3: Build the manual control screen

**Files:**
- Create: `Android/app/src/main/java/com/example/volantemeteoro/MainActivity.java`
- Create: `Android/app/src/main/res/layout/activity_main.xml`
- Modify: `Android/app/src/main/res/values/strings.xml`

- [ ] **Step 1: Add fields for the three thresholds and a manual alarm button**
- [ ] **Step 2: Wire the button to publish `UMBRAL_MANO`, `UMBRAL_LEVE`, and `UMBRAL_BRUSCO`**
- [ ] **Step 3: Publish `ALARMA` when requested**
- [ ] **Step 4: Show connection and publish status on screen**

### Task 4: Build the GPS speed screen

**Files:**
- Create: `Android/app/src/main/java/com/example/volantemeteoro/GpsActivity.java`
- Create: `Android/app/src/main/res/layout/activity_gps.xml`
- Modify: `Android/app/src/main/AndroidManifest.xml`

- [ ] **Step 1: Request location permission at runtime**
- [ ] **Step 2: Read speed from `LocationManager` and display it**
- [ ] **Step 3: Publish speed updates to MQTT on a fixed interval**
- [ ] **Step 4: Add a navigation button from the first screen**

### Task 5: Final validation

**Files:**
- Modify: `Android/app/src/main/java/com/example/volantemeteoro/MainActivity.java`
- Modify: `Android/app/src/main/java/com/example/volantemeteoro/GpsActivity.java`

- [ ] **Step 1: Check that both Activities build against the same helper**
- [ ] **Step 2: Verify topic names match the ESP32 sketch**
- [ ] **Step 3: Run a local Gradle build if the Android toolchain is available**
- [ ] **Step 4: Fix any resource or manifest errors found during validation**
