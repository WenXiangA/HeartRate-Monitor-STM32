# -*- coding: utf-8 -*-
"""
心率算法验证：证明 HeartRate-Monitor-STM32 中 get_BMP() 的死代码问题，
并对比多种修复方案在全心率区间的表现，选定最终方案。

把 adc.c 的算法逐行等价翻译成 Python，喂入模拟的 PPG（光电容积脉搏波）信号。

运行： python verify/bpm_fix_verify.py
"""

import math

SAMPLE_PERIOD_MS = 10
PULSE_TIMEOUT = 250          # 250 * 10ms = 2.5s 无脉搏
BPM_MIN, BPM_MAX = 30, 200
SIGNAL_MIN_AMPL = 20


class PPG:
    """模拟 PulseSensor 输出：基线 + 周期性脉搏波 + 轻微噪声"""

    def __init__(self, bpm=None, baseline=2048, amplitude=400, noise=3, flat=False):
        self.period_ms = 60000.0 / bpm if bpm else 1000.0
        self.baseline = baseline
        self.amplitude = amplitude
        self.noise = noise
        self.flat = flat
        self._seed = 1

    def _rand(self):
        self._seed = (self._seed * 1103515245 + 12345) & 0x7FFFFFFF
        return ((self._seed >> 16) % 1000) / 1000.0 - 0.5

    def value(self, t_ms):
        if self.flat:
            return int(self.baseline + self._rand() * 2 * self.noise)
        p = (t_ms % self.period_ms) / self.period_ms
        g = math.exp(-((p - 0.25) ** 2) / (2 * 0.08 ** 2))
        v = self.baseline + self.amplitude * g + self._rand() * 2 * self.noise
        return int(max(0, min(4095, v)))


def new_state(sig, window_size):
    return {
        'sig': sig, 'W': window_size, 'readData': 0, 'data': [0] * window_size,
        'index_pulse': 0, 'max': 0, 'min': 0, 'mid': 2048, 'filter': 0,
        'BPM': 0, 'IBI': 600, 'timeCount': 0,
        'firstTimeCount': 0, 'secondTimeCount': 0, 'pulseCount': 0,
        'PULSE': False, 'PRE_PULSE': False,
        'window_ready': 0, 'signal_ok': 0, 'updates': 0, 't': 0, 'edge_valid': 0,
    }


# ---------------------------------------------------------------------------
# 修复前：原始 get_BMP()
# ---------------------------------------------------------------------------
def get_bmp_old(s):
    W = s['W']
    s['readData'] = s['sig'].value(s['t'])
    s['data'][s['index_pulse']] = s['readData']
    s['index_pulse'] += 1

    if s['index_pulse'] >= W:
        s['index_pulse'] = 0
        s['max'] = max(s['data']); s['min'] = min(s['data'])
        if (s['max'] - s['min']) < SIGNAL_MIN_AMPL:
            s['mid'] = 4095; s['BPM'] = 0
        else:
            s['mid'] = (s['max'] + s['min']) // 2

    # 原代码：index_pulse 刚被归零，此处恒为真 → 以下全部不可达
    if s['index_pulse'] < W:
        s['timeCount'] += 1
        return

    s['PRE_PULSE'] = s['PULSE']
    s['PULSE'] = s['readData'] > s['mid']
    if s['PRE_PULSE'] is False and s['PULSE'] is True:
        s['pulseCount'] = (s['pulseCount'] + 1) % 2
        if s['pulseCount'] == 1:
            s['firstTimeCount'] = s['timeCount']
        if s['pulseCount'] == 0:
            s['secondTimeCount'] = s['timeCount']
            s['timeCount'] = 0
            if s['secondTimeCount'] > s['firstTimeCount']:
                s['IBI'] = (s['secondTimeCount'] - s['firstTimeCount']) * 10
                if s['IBI'] > 0:
                    nb = 60000 // s['IBI']
                    if BPM_MIN <= nb <= BPM_MAX:
                        s['BPM'] = nb; s['updates'] += 1
    s['timeCount'] += 1


# ---------------------------------------------------------------------------
# 修复方案：window_size 可调 + reset_on_weak 可选
# ---------------------------------------------------------------------------
def reset_pulse_detect(s, full):
    s['BPM'] = 0
    s['timeCount'] = 0
    s['pulseCount'] = 0
    s['PULSE'] = False
    s['PRE_PULSE'] = False
    s['edge_valid'] = 0
    if full:                      # 连窗口一起重置 → 需要重新预热
        s['index_pulse'] = 0
        s['window_ready'] = 0
        s['signal_ok'] = 0


def get_bpm_new(s, reset_on_weak):
    """方案 B/C：窗口可调，每个心搏间隔都算一次 IBI"""
    W = s['W']
    s['readData'] = s['sig'].value(s['t'])
    s['data'][s['index_pulse']] = s['readData']
    s['index_pulse'] += 1

    if s['index_pulse'] >= W:
        s['index_pulse'] = 0
        s['window_ready'] = 1
        s['max'] = max(s['data']); s['min'] = min(s['data'])
        if (s['max'] - s['min']) < SIGNAL_MIN_AMPL:
            s['signal_ok'] = 0
        else:
            s['signal_ok'] = 1
            s['mid'] = (s['max'] + s['min']) // 2
            s['filter'] = (s['max'] - s['min']) // 2

    if s['window_ready'] == 0:
        s['timeCount'] = 0
        return
    if s['signal_ok'] == 0:
        # 只清显示值并重新对齐，不重置窗口（避免低速心率下反复重新预热）
        s['BPM'] = 0
        s['timeCount'] = 0
        s['edge_valid'] = 0
        s['PULSE'] = False
        s['PRE_PULSE'] = False
        return

    s['PRE_PULSE'] = s['PULSE']
    s['PULSE'] = s['readData'] > s['mid']
    if s['PRE_PULSE'] is False and s['PULSE'] is True:
        # 每个上升沿都清零：timeCount 即"距上次脉搏的间隔"
        if s['edge_valid'] == 0:
            s['edge_valid'] = 1          # 第一个沿只做对齐，不计算
            s['timeCount'] = 0
        else:
            s['IBI'] = s['timeCount'] * SAMPLE_PERIOD_MS
            s['timeCount'] = 0
            if s['IBI'] > 0:
                nb = 60000 // s['IBI']
                if BPM_MIN <= nb <= BPM_MAX:
                    s['BPM'] = nb; s['updates'] += 1

    if s['timeCount'] > PULSE_TIMEOUT:
        reset_pulse_detect(s, full=True)
        return
    s['timeCount'] += 1


def run(fn, true_bpm, window, reset_on_weak, seconds=60, flat=False):
    sig = PPG(true_bpm, flat=flat)
    s = new_state(sig, window)
    steps = seconds * 1000 // SAMPLE_PERIOD_MS
    samples = []
    for i in range(steps):
        s['t'] = i * SAMPLE_PERIOD_MS
        fn(s, reset_on_weak) if fn is get_bpm_new else fn(s)
        if i > steps // 2:
            samples.append(s['BPM'])
    nz = [v for v in samples if v > 0]
    return {
        'avg': (sum(nz) / len(nz)) if nz else 0,
        'ratio': len(nz) / len(samples),
        'updates': s['updates'],
    }


if __name__ == '__main__':
    cases = [30, 40, 50, 60, 72, 90, 120, 150, 180, 200]
    plans = [
        ('修复前  get_BMP (W=50)', get_bmp_old, 50, None),
        ('方案B  W=50  每沿计数', get_bpm_new, 50, False),
        ('方案D  W=150 每沿计数', get_bpm_new, 150, False),
        ('方案E  W=250 每沿计数', get_bpm_new, 250, False),
    ]

    print("采样周期 10ms，每个心率点模拟 60 秒，取后半段稳态\n")
    header = f"{'方案':<24}" + "".join(f"{b:<7}" for b in cases)
    print(header)
    print("-" * len(header))
    for name, fn, w, row in plans:
        line = f"{name:<24}"
        for bpm in cases:
            d = run(fn, bpm, w, row)
            ok = abs(d['avg'] - bpm) < 6 and d['ratio'] > 0.5
            line += f"{('%.0f' % d['avg']) if d['avg'] else '-':<7}" if ok else f"{'✗':<7}"
        print(line)
    print("-" * len(header))
    print("（显示数值=稳态均值；✗ = 未检出或偏差>6 BPM）\n")

    print("详细数据（方案E  W=250）：")
    print(f"{'真实BPM':<10}{'稳态均值':<12}{'有效输出占比':<14}{'脉搏检测次数'}")
    for bpm in cases:
        d = run(get_bpm_new, bpm, 250, False)
        print(f"{bpm:<10}{round(d['avg'],1):<12}{round(d['ratio']*100,1):<14}{d['updates']}")

    print("\n误报检查（传感器未佩戴 / 平直信号，期望 BPM 恒为 0）：")
    d_old = run(get_bmp_old, None, 50, None, flat=True)
    d_c = run(get_bpm_new, None, 250, False, flat=True)
    print(f"  修复前：{d_old['avg']:.0f}（检测 {d_old['updates']} 次）")
    print(f"  方案E ：{d_c['avg']:.0f}（检测 {d_c['updates']} 次）"
          f"  -> {'无误报' if d_c['updates'] == 0 else '存在误报'}")
