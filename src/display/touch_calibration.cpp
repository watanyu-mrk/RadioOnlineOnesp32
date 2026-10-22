#include "display/touch_calibration.h"

#include <math.h>

// 3x3 measurement grid: nine targets, corners included.
static const int16_t TARGET_X[TouchCalibration::POINTS] = {80, 240, 400, 80, 240,
                                                           400, 80, 240, 400};
static const int16_t TARGET_Y[TouchCalibration::POINTS] = {80, 80, 80, 240, 240,
                                                           240, 400, 400, 400};

// 3x3 linear solve with partial pivoting (used by the affine fit).
static bool solve3(const float in[3][3], const float rhs[3], float out[3]) {
  float m[3][4];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) m[i][j] = in[i][j];
    m[i][3] = rhs[i];
  }
  for (int col = 0; col < 3; col++) {
    int piv = col;
    for (int r = col + 1; r < 3; r++) {
      if (fabsf(m[r][col]) > fabsf(m[piv][col])) piv = r;
    }
    if (fabsf(m[piv][col]) < 1e-4f) return false;
    if (piv != col) {
      for (int j = 0; j < 4; j++) {
        float t = m[col][j];
        m[col][j] = m[piv][j];
        m[piv][j] = t;
      }
    }
    for (int r = 0; r < 3; r++) {
      if (r == col) continue;
      float f = m[r][col] / m[col][col];
      for (int j = col; j < 4; j++) m[r][j] -= f * m[col][j];
    }
  }
  for (int i = 0; i < 3; i++) out[i] = m[i][3] / m[i][i];
  return true;
}

// Barycentric weights of (px,py) inside the raw triangle (ia,ib,ic).
static bool bary(const TouchCalData &d, float px, float py, int ia, int ib, int ic, float &w0,
                 float &w1, float &w2) {
  float ax = (float)d.rawX[ia], ay = (float)d.rawY[ia];
  float bx = (float)d.rawX[ib], by = (float)d.rawY[ib];
  float cx = (float)d.rawX[ic], cy = (float)d.rawY[ic];
  float det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
  if (fabsf(det) < 1e-3f) return false;
  w1 = ((px - ax) * (cy - ay) - (cx - ax) * (py - ay)) / det;
  w2 = ((bx - ax) * (py - ay) - (px - ax) * (by - ay)) / det;
  w0 = 1.0f - w1 - w2;
  return (w0 >= -0.05f && w1 >= -0.05f && w2 >= -0.05f);
}

void TouchCalibration::begin(SettingsStore *store) {
  store_ = store;
  have_ = store_ && store_->loadTouchCal(data_);
  Serial.printf("touch     : calibration %s\n", have_ ? "loaded from NVS" : "missing");
}

int16_t TouchCalibration::targetX(uint8_t i) {
  return TARGET_X[i < POINTS ? i : 0];
}

int16_t TouchCalibration::targetY(uint8_t i) {
  return TARGET_Y[i < POINTS ? i : 0];
}

void TouchCalibration::start() {
  active_ = true;
  step_ = 0;
  Serial.println("cal       : touch the nine crosses (TL, TC, TR, ML, MC, MR, BL, BC, BR)");
}

// Inside the calibrated grid the containing triangle is interpolated; outside it, the
// least-squares affine fallback is used.
void TouchCalibration::map(uint16_t rawX, uint16_t rawY, uint16_t &sx, uint16_t &sy) const {
  // No measured grid yet: the GT911 point bytes already are screen pixels on this panel,
  // so the mapping is the identity. (data_.aff is all zeros in that state - using it here
  // collapsed every tap to 0,0.)
  if (!have_) {
    sx = rawX > 479 ? 479 : rawX;
    sy = rawY > 479 ? 479 : rawY;
    return;
  }
  for (int gy = 0; gy < 2; gy++) {
    for (int gx = 0; gx < 2; gx++) {
      int k = gy * 3 + gx;  // tl=0 tr=1 bl=2 br=3 within the cell
      int idx[4] = {k, k + 1, k + 3, k + 4};
      const int tri[2][3] = {{0, 1, 2}, {1, 3, 2}};
      for (int t = 0; t < 2; t++) {
        int a = idx[tri[t][0]], b = idx[tri[t][1]], c = idx[tri[t][2]];
        float w0, w1, w2;
        if (!bary(data_, (float)rawX, (float)rawY, a, b, c, w0, w1, w2)) continue;
        float x = w0 * TARGET_X[a] + w1 * TARGET_X[b] + w2 * TARGET_X[c];
        float y = w0 * TARGET_Y[a] + w1 * TARGET_Y[b] + w2 * TARGET_Y[c];
        sx = (uint16_t)constrain((int)lroundf(x), 0, 479);
        sy = (uint16_t)constrain((int)lroundf(y), 0, 479);
        return;
      }
    }
  }
  sx = (uint16_t)constrain((int)lroundf(data_.aff[0] * rawX + data_.aff[1] * rawY + data_.aff[2]),
                           0, 479);
  sy = (uint16_t)constrain((int)lroundf(data_.aff[3] * rawX + data_.aff[4] * rawY + data_.aff[5]),
                           0, 479);
}

bool TouchCalibration::sample(uint16_t rawX, uint16_t rawY) {
  if (!active_ || step_ >= POINTS) return false;
  data_.rawX[step_] = rawX;
  data_.rawY[step_] = rawY;
  Serial.printf("cal       : target %u at (%d,%d) -> raw=(%u,%u)\n", (unsigned)step_ + 1,
                TARGET_X[step_], TARGET_Y[step_], rawX, rawY);
  step_++;
  if (step_ < POINTS) return false;
  active_ = false;
  finish();
  return true;
}

// Fits the affine fallback from the measured grid and stores the grid itself.
void TouchCalibration::finish() {
  float Sxx = 0, Sxy = 0, Sx = 0, Syy = 0, Sy = 0;
  float rxSx = 0, rySx = 0, sumSx = 0;
  float rxSy = 0, rySy = 0, sumSy = 0;
  for (uint8_t i = 0; i < POINTS; i++) {
    float rx = (float)data_.rawX[i], ry = (float)data_.rawY[i];
    float sx = (float)TARGET_X[i], sy = (float)TARGET_Y[i];
    Sxx += rx * rx;
    Sxy += rx * ry;
    Sx += rx;
    Syy += ry * ry;
    Sy += ry;
    rxSx += rx * sx;
    rySx += ry * sx;
    sumSx += sx;
    rxSy += rx * sy;
    rySy += ry * sy;
    sumSy += sy;
  }

  const float normal[3][3] = {{Sxx, Sxy, Sx}, {Sxy, Syy, Sy}, {Sx, Sy, (float)POINTS}};
  const float rhsX[3] = {rxSx, rySx, sumSx};
  const float rhsY[3] = {rxSy, rySy, sumSy};
  float vx[3] = {0}, vy[3] = {0};
  if (!solve3(normal, rhsX, vx) || !solve3(normal, rhsY, vy)) {
    Serial.println("cal       : FAILED (degenerate fit) - send 'k' to retry");
    step_ = 0;
    return;
  }

  float maxErr = 0;
  for (uint8_t i = 0; i < POINTS; i++) {
    float px = vx[0] * data_.rawX[i] + vx[1] * data_.rawY[i] + vx[2];
    float py = vy[0] * data_.rawX[i] + vy[1] * data_.rawY[i] + vy[2];
    float ex = fabsf(px - TARGET_X[i]);
    float ey = fabsf(py - TARGET_Y[i]);
    if (ex > maxErr) maxErr = ex;
    if (ey > maxErr) maxErr = ey;
  }

  data_.aff[0] = vx[0];
  data_.aff[1] = vx[1];
  data_.aff[2] = vx[2];
  data_.aff[3] = vy[0];
  data_.aff[4] = vy[1];
  data_.aff[5] = vy[2];
  data_.valid = true;
  have_ = true;
  if (store_) store_->saveTouchCal(data_);

  Serial.printf("cal       : affine a=%.4f b=%.4f c=%.1f | d=%.4f e=%.4f f=%.1f (saved)\n",
                vx[0], vx[1], vx[2], vy[0], vy[1], vy[2]);
  Serial.printf("cal       : grid raw (3x3, affine residual %.1f px)\n", maxErr);
  for (uint8_t i = 0; i < POINTS; i++) {
    Serial.printf("cal       :  target(%3d,%3d) -> raw(%3u,%3u)\n", TARGET_X[i], TARGET_Y[i],
                  data_.rawX[i], data_.rawY[i]);
  }
}
