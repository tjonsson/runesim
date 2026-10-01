"""Synthesize original combat, bird-call and surface footstep sounds (standard library only).

All output is procedurally generated for this project (no third-party recordings), deterministic
per seed, mono 16-bit 48 kHz. Writes to Art/LivingWorld/Prepared/Audio/{Combat,Birds,Footsteps}.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
ROOT = Path(__file__).resolve().parents[1] / 'Art/LivingWorld/Prepared/Audio'


def write(path, samples, peak=0.89):
    path.parent.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    scale = peak / top
    with wave.open(str(path), 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, s * scale)) * 32767)) for s in samples))


class OnePole:
    """One-pole low-pass; cutoff may change per sample."""
    def __init__(self):
        self.y = 0.0

    def __call__(self, x, cutoff):
        a = math.exp(-2 * math.pi * max(10.0, cutoff) / RATE)
        self.y = (1 - a) * x + a * self.y
        return self.y


class Biquad:
    """RBJ band-pass (constant peak gain)."""
    def __init__(self, freq, q):
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0
        self.set(freq, q)

    def set(self, freq, q):
        w = 2 * math.pi * freq / RATE
        alpha = math.sin(w) / (2 * q)
        a0 = 1 + alpha
        self.b0, self.b1, self.b2 = alpha / a0, 0.0, -alpha / a0
        self.a1, self.a2 = -2 * math.cos(w) / a0, (1 - alpha) / a0

    def __call__(self, x):
        y = self.b0 * x + self.b1 * self.x1 + self.b2 * self.x2 - self.a1 * self.y1 - self.a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


def envelope(t, attack, decay):
    return (t / attack if t < attack else math.exp(-(t - attack) / decay))


def explosion(seed):
    rng = random.Random(seed)
    n = int(RATE * 4.5)
    body, air, crackle = OnePole(), OnePole(), OnePole()
    phase = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        # Supersonic crack, then a pressure wave whose spectrum darkens as it travels.
        crack = noise * math.exp(-t / 0.006) * 1.4
        roar = body(noise, 4000 * math.exp(-t / 0.35) + 120) * envelope(t, 0.004, 0.9) * 2.2
        freq = 38 + 55 * math.exp(-t / 0.25)
        phase += 2 * math.pi * freq / RATE
        boom = math.sin(phase) * envelope(t, 0.01, 0.7) * 1.1
        # Late debris and rolling echo.
        debris = crackle(noise if rng.random() < 0.004 * math.exp(-t / 1.2) else 0.0, 3000) * 25
        echo = air(noise, 600) * math.exp(-((t - 0.9) / 0.9) ** 2) * 0.6 if t > 0.3 else 0.0
        out.append(crack + roar + boom + debris + echo)
    return out


def missile_launch(seed):
    rng = random.Random(seed)
    n = int(RATE * 2.2)
    lp, hp_state = OnePole(), OnePole()
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        ignition = noise * math.exp(-t / 0.015) * 1.2
        # Booster roar recedes (distance + Doppler darkening) as the round leaves the rail.
        cutoff = 5500 * math.exp(-t / 0.8) + 400
        roar = lp(noise, cutoff) * envelope(t, 0.03, 0.9) * 2.5
        hiss = (noise - hp_state(noise, 2500)) * envelope(t, 0.02, 0.35) * 0.4
        thump = math.sin(2 * math.pi * 55 * t) * math.exp(-t / 0.12) * 0.8
        out.append(ignition + roar + hiss + thump)
    return out


def missile_motor(seed, seconds=2.0):
    rng = random.Random(seed)
    n = int(RATE * seconds)
    lp, lp2 = OnePole(), OnePole()
    raw = []
    for i in range(n):
        noise = rng.uniform(-1, 1)
        pops = 18.0 if rng.random() < 0.0015 else 0.0
        raw.append(lp(noise, 2800) * 1.6 + lp2(noise * 0.4 + pops, 900))
    # Seamless loop: crossfade the tail into the head.
    fade = int(RATE * 0.25)
    for i in range(fade):
        w = i / fade
        raw[i] = raw[i] * w + raw[n - fade + i] * (1 - w)
    return raw[:n - fade]


def pigeon_coo(seed):
    """Rock dove: soft 'coo-roo-coo', fundamental ~300-450 Hz, breathy."""
    rng = random.Random(seed)
    pitch = rng.uniform(0.92, 1.08)
    syllables = [(0.0, 0.28, 440, 330), (0.34, 0.42, 380, 300), (0.82, 0.3, 420, 320)]
    n = int(RATE * 1.25)
    out = [0.0] * n
    breath = OnePole()
    for start, length, f0, f1 in syllables:
        phase = 0.0
        for i in range(int(length * RATE)):
            t = i / RATE
            k = t / length
            freq = (f0 + (f1 - f0) * k) * pitch * (1 + 0.025 * math.sin(2 * math.pi * 22 * t))
            phase += 2 * math.pi * freq / RATE
            amp = math.sin(math.pi * min(1, k * 1.2)) ** 1.5
            tone = math.sin(phase) + 0.35 * math.sin(2 * phase) + 0.12 * math.sin(3 * phase)
            idx = int((start + t) * RATE)
            if idx < n:
                out[idx] += amp * (tone + breath(rng.uniform(-1, 1), 900) * 0.6)
    return out


def gull_call(seed):
    """Herring-gull style 'kyow' calls: bright, harmonic, falling pitch, repeated."""
    rng = random.Random(seed)
    count = rng.choice([2, 3, 3, 4])
    n = int(RATE * (0.2 + count * 0.34))
    out = [0.0] * n
    formant, formant2 = Biquad(2400, 3), Biquad(1300, 4)
    for c in range(count):
        start = 0.05 + c * rng.uniform(0.3, 0.36)
        length = rng.uniform(0.22, 0.28)
        f0 = rng.uniform(1150, 1350) * (0.97 ** c)
        phase = 0.0
        for i in range(int(length * RATE)):
            t = i / RATE
            k = t / length
            freq = f0 * (1.0 + 0.25 * math.sin(math.pi * min(1, k * 1.6))) * (1 - 0.45 * k)
            phase += 2 * math.pi * freq / RATE
            saw = sum(math.sin(h * phase) / h for h in range(1, 9))
            amp = min(1, k / 0.05) * (1 - k) ** 0.6
            idx = int((start + t) * RATE)
            if idx < n:
                out[idx] += amp * (formant(saw) * 2 + formant2(saw) + 0.1 * rng.uniform(-1, 1))
    return out


def crow_caw(seed):
    """Carrion crow: harsh nasal 'kraa', buzzy harmonics with noise, 2-4 repeats."""
    rng = random.Random(seed)
    count = rng.choice([2, 3, 3, 4])
    n = int(RATE * (0.15 + count * 0.5))
    out = [0.0] * n
    nasal, throat = Biquad(1600, 2.5), Biquad(750, 3)
    for c in range(count):
        start = 0.05 + c * rng.uniform(0.42, 0.52)
        length = rng.uniform(0.3, 0.38)
        f0 = rng.uniform(480, 560)
        phase = 0.0
        for i in range(int(length * RATE)):
            t = i / RATE
            k = t / length
            freq = f0 * (1 + 0.12 * math.sin(math.pi * k)) * (1 + 0.04 * rng.uniform(-1, 1))
            phase += 2 * math.pi * freq / RATE
            buzz = sum(math.sin(h * phase) / h ** 0.8 for h in range(1, 12))
            rough = buzz * (1 + 0.6 * math.sin(2 * math.pi * 70 * t)) + rng.uniform(-1, 1) * 1.2
            amp = min(1, k / 0.04) * (1 - k) ** 0.8
            idx = int((start + t) * RATE)
            if idx < n:
                out[idx] += amp * (nasal(rough) * 1.6 + throat(rough))
    return out


def footstep(seed, surface):
    """Heel strike + roll-off with surface texture: dirt thud, gravel crunch, grass rustle."""
    rng = random.Random(seed)
    n = int(RATE * 0.45)
    lp, lp2, grain = OnePole(), OnePole(), OnePole()
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        heel = lp(noise, 700) * envelope(t, 0.002, 0.03) * 3
        roll = lp2(noise, 1500) * math.exp(-((t - 0.1) / 0.035) ** 2) * 1.2
        if surface == 'dirt':
            s = heel * 1.3 + roll + math.sin(2 * math.pi * 90 * t) * math.exp(-t / 0.04) * 0.5
        elif surface == 'gravel':
            crunch = grain(noise if rng.random() < 0.08 * math.exp(-t / 0.12) else 0, 4500) * 6
            s = heel * 0.8 + crunch + roll * 0.6
        else:  # grass
            rustle = (noise - grain(noise, 1800)) * (envelope(t, 0.01, 0.07) + 0.4 * math.exp(-((t - 0.11) / 0.04) ** 2))
            s = heel * 0.5 + rustle * 0.8
        out.append(s)
    return out


def main():
    combat = ROOT / 'Combat'
    write(combat / 'missile_launch.wav', missile_launch(11))
    write(combat / 'missile_motor.wav', missile_motor(12), peak=0.7)
    write(combat / 'explosion.wav', explosion(13))
    birds = ROOT / 'Birds'
    for i in range(3):
        write(birds / f'pigeon_coo_{i}.wav', pigeon_coo(100 + i), peak=0.6)
        write(birds / f'gull_call_{i}.wav', gull_call(200 + i), peak=0.7)
        write(birds / f'crow_caw_{i}.wav', crow_caw(300 + i), peak=0.7)
    steps = ROOT / 'Footsteps'
    for surface in ('dirt', 'gravel', 'grass'):
        for i in range(5):
            write(steps / f'footstep_{surface}_{i:03d}.wav', footstep(400 + i * 7 + len(surface), surface), peak=0.5)
    print('SYNTHESIZED', sorted(str(p.relative_to(ROOT)) for p in ROOT.rglob('*.wav') if p.parent.name in ('Combat', 'Birds', 'Footsteps')))


if __name__ == '__main__':
    main()
