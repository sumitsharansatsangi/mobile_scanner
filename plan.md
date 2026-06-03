
# Strategy to use

## Primary: ZXing-C++

## Fallback: ML Kit

NOT the other way around.

---

# Why ZXing First?

Because ZXing-C++ gives us:

* lower latency
* lower CPU usage
* smaller memory footprint
* cross-platform support
* fully native Rust integration
* no JNI/platform-channel overhead

Most QR scans are easy.

So you want the lightweight decoder first.

---

# Then Use ML Kit Only When Needed

Use ML Kit when:

* ZXing fails
* frame is blurry
* low light
* rotated heavily
* damaged QR
* perspective distortion
* tiny QR in large image

This gives us:

* near-ML-Kit accuracy
* ZXing performance for normal scans

Best of both worlds.

---

# Industry-Style Hybrid Pipeline

```text id="o80otj"
Camera Frame
     ↓
Preprocessing
     ↓
ZXing-C++ (Fast Path)
     ↓ success?
   YES → Return Result
     ↓ NO
ML Kit (Recovery Path)
     ↓
Return Result
```

---

# Huge Practical Benefit

This architecture dramatically reduces:

* battery drain
* thermal throttling
* frame drops

because ML Kit does not run continuously.

Instead:

* ZXing handles ~90–98% scans
* ML Kit handles difficult edge cases

---

# Another Big Advantage

You can still support:

| Platform | ZXing | ML Kit |
| -------- | ----- | ------ |
| Android  | ✅     | ✅      |
| iOS      | ✅     | ✅      |
| macOS    | ✅     | ❌      |
| Linux    | ✅     | ❌      |
| Windows  | ✅     | ❌      |

Meaning:

* desktop always uses ZXing
* mobile gets intelligent fallback

Excellent architecture.

---

# Smart Triggering Strategy

Do NOT immediately call ML Kit after one failure.

Instead:

```text id="f0i7w0"
ZXing failed 3 consecutive frames
        OR
frame luminance too low
        OR
camera autofocus unstable
        OR
QR candidate detected but decode failed
→ invoke ML Kit
```

This prevents:

* excessive CPU usage
* camera lag
* overheating

---

# Even Better Architecture

We can use:

## Stage 1: QR Region Detection

(OpenCV or ML Kit)

## Stage 2: ZXing Decode

This is extremely powerful.

Reason:

ZXing's weakness is often:

* locating QR under difficult conditions

NOT decoding itself.

So:

* ML/OpenCV finds QR bounds
* ZXing decodes cropped region

This can outperform using ML Kit alone.

---

# Ideal Architecture For You

Since you already use Rust:

```text id="43bj0x"
Flutter Camera
      ↓
Rust Frame Processor
      ↓
ZXing-C++ decode
      ↓ fail?
OpenCV enhancement
      ↓ retry
ZXing-C++ retry
      ↓ fail?
Platform ML Kit
      ↓
Result
```

This is very close to how high-performance scanner apps are built.

---

# Important Performance Tip

Avoid sending full-resolution frames to ML Kit continuously.

Instead:

* Run ZXing on grayscale downscaled frames
* Invoke ML Kit only occasionally
* Crop likely QR regions before ML Kit

This massively improves FPS.

---

# Real-World Outcome

With hybrid architecture you can achieve:

| Metric                     | Result |
| -------------------------- | ------ |
| Fast scans                 | ZXing  |
| Difficult scans            | ML Kit |
| Battery                    | Better |
| Desktop support            | Yes    |
| Thermal efficiency         | Better |
| Cross-platform consistency | Better |

---

# One More Advanced Optimization

Use confidence scoring.

Example:

```rust
enum ScanResult {
    Strong(String),
    WeakCandidate,
    None,
}
```

If ZXing returns:

* weak candidate
* partial finder patterns
* checksum mismatch

→ trigger ML Kit immediately.

Very efficient.

---
You would implement:

## Core Engine

* ZXing-C++ in Rust

## Enhancement Layer

* OpenCV preprocessing

## Mobile Recovery Layer

* ML Kit fallback

That gives us:

* maximum control
* best performance
* best portability
* best accuracy overall
