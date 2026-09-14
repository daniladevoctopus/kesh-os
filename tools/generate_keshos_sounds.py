import wave
import math
import struct
import os
import shutil

def generate_startup_chime(output_path, duration=3.8, sample_rate=44100):
    # Rich polyphonic major chord inspired by the classic Mac startup chime
    # Partials: (frequency_hz, amplitude, decay_rate, stereo_pan -1..1)
    partials = [
        # Deep warm sub/bass
        (92.5, 0.45, 1.1, 0.0),      # F#2
        (138.6, 0.35, 1.2, -0.1),    # C#3
        # Body
        (185.0, 0.50, 0.95, 0.1),    # F#3
        (233.1, 0.46, 1.05, -0.2),   # A#3
        (277.2, 0.42, 1.15, 0.2),    # C#4
        # Treble chime & sparkle
        (370.0, 0.42, 1.35, -0.25),  # F#4
        (466.2, 0.36, 1.45, 0.25),   # A#4
        (554.4, 0.32, 1.55, -0.3),   # C#5
        (740.0, 0.26, 1.85, 0.3),    # F#5
        (932.3, 0.18, 2.15, -0.35),  # A#5
        (1108.7, 0.14, 2.45, 0.35),  # C#6
        (1480.0, 0.10, 2.85, 0.0),   # F#6
        (1864.6, 0.06, 3.20, -0.2),  # A#6
        (2217.4, 0.04, 3.60, 0.2),   # C#7
    ]
    
    num_samples = int(duration * sample_rate)
    left_channel = [0.0] * num_samples
    right_channel = [0.0] * num_samples
    
    for freq, amp, decay, pan in partials:
        # Slight stereo chorus detuning
        freq_l = freq * (1.0 - 0.0012)
        freq_r = freq * (1.0 + 0.0012)
        
        # Pan law
        pan_l = math.cos((pan + 1.0) * math.pi / 4.0)
        pan_r = math.sin((pan + 1.0) * math.pi / 4.0)
        
        for i in range(num_samples):
            t = i / sample_rate
            
            # Smooth attack envelope (6ms smooth rise), exponential decay
            env = (1.0 - math.exp(-t * 220.0)) * math.exp(-t * decay)
            
            # Fundamental + subtle warm second harmonic
            sig_l = math.sin(2.0 * math.pi * freq_l * t) + 0.22 * math.sin(4.0 * math.pi * freq_l * t)
            sig_r = math.sin(2.0 * math.pi * freq_r * t) + 0.22 * math.sin(4.0 * math.pi * freq_r * t)
            
            # Soft saturation warmth
            sig_l = math.tanh(sig_l * 0.9)
            sig_r = math.tanh(sig_r * 0.9)
            
            left_channel[i] += amp * env * sig_l * pan_l
            right_channel[i] += amp * env * sig_r * pan_r
            
    # Normalize to -0.8 dBFS peak
    max_val = max(max(abs(s) for s in left_channel), max(abs(s) for s in right_channel), 0.001)
    scale = 32767.0 * 0.90 / max_val
    
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with wave.open(output_path, 'wb') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        
        frames = bytearray()
        for l, r in zip(left_channel, right_channel):
            s_l = int(max(-32768, min(32767, l * scale)))
            s_r = int(max(-32768, min(32767, r * scale)))
            frames.extend(struct.pack('<hh', s_l, s_r))
        wf.writeframes(frames)
    print(f'Created startup chime: {output_path} ({os.path.getsize(output_path)} bytes)')

def generate_error_sound(output_path, duration=0.34, sample_rate=44100):
    # Tactile, warm wooden/percussive alert sound
    num_samples = int(duration * sample_rate)
    left_channel = [0.0] * num_samples
    right_channel = [0.0] * num_samples
    
    for i in range(num_samples):
        t = i / sample_rate
        
        # 1. Transient click at onset (first 4ms)
        click_env = math.exp(-t * 800.0)
        click = math.sin(2.0 * math.pi * 1400.0 * t) * click_env * 0.35
        
        # 2. Low punch thud (180 Hz dropping to 80 Hz)
        thud_freq = 80.0 + 100.0 * math.exp(-t * 45.0)
        thud_env = (1.0 - math.exp(-t * 350.0)) * math.exp(-t * 22.0)
        thud = math.sin(2.0 * math.pi * thud_freq * t) * thud_env * 0.75
        
        # 3. Wooden body resonance (dual harmonic peaks at 380 Hz and 760 Hz)
        res_env = (1.0 - math.exp(-t * 400.0)) * math.exp(-t * 18.0)
        res = (math.sin(2.0 * math.pi * 380.0 * t) * 0.55 + 
               math.sin(2.0 * math.pi * 760.0 * t) * 0.28) * res_env
               
        sample = click + thud + res
        # Soft clipping
        sample = math.tanh(sample * 1.1)
        
        left_channel[i] = sample
        right_channel[i] = sample
        
    max_val = max(abs(s) for s in left_channel) or 0.001
    scale = 32767.0 * 0.88 / max_val
    
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with wave.open(output_path, 'wb') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        
        frames = bytearray()
        for l, r in zip(left_channel, right_channel):
            s_l = int(max(-32768, min(32767, l * scale)))
            s_r = int(max(-32768, min(32767, r * scale)))
            frames.extend(struct.pack('<hh', s_l, s_r))
        wf.writeframes(frames)
    print(f'Created error sound: {output_path} ({os.path.getsize(output_path)} bytes)')

if __name__ == '__main__':
    base_dir = r'c:\Users\DanDevXP\Desktop\keshoos\media\sounds'
    
    startup_wav = os.path.join(base_dir, 'KeshOS_Startup.wav')
    error_wav = os.path.join(base_dir, 'KeshOS_Error.wav')
    
    generate_startup_chime(startup_wav)
    generate_error_sound(error_wav)
    
    # Also overwrite the legacy files so any component requesting them gets the new sound
    legacy_startup = os.path.join(base_dir, 'ReactOS_Startup.wav')
    legacy_stop = os.path.join(base_dir, 'ReactOS_Critical_Stop.wav')
    
    shutil.copyfile(startup_wav, legacy_startup)
    shutil.copyfile(error_wav, legacy_stop)
    print('Updated legacy startup and critical stop wavs.')
