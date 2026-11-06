"""SYNTHETIC ADC loopback — never real sensor calibration."""
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'tools'))
from ChannelLatency18 import measure

r=np.random.default_rng(1818)
base=r.normal(0,.025,size=4096)
wave=np.stack([base,np.r_[np.zeros(3),base[:-3]],np.r_[base[4:],np.zeros(4)],
               np.r_[np.zeros(12),base[:-12]]],axis=-1)
report=measure(wave,48000)
result=[x['delay_vs_ch1_samples'] for x in report['channel_results']]
print('SYNTHETIC latency results:',result)
assert np.max(np.abs(np.asarray(result)-[0,3,-4,12]))<.1
assert not report['internal_sensor_delay_calibrated']
print('SYNTHETIC latency test PASS')
