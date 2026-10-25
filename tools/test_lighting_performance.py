"""Check that benchmark reports retain real stalls and separate capture overhead."""
import pytest
from lighting_performance_run import distribution, summarize


def test_percentiles_and_slow_frame_budget():
    result=distribution([10]*98+[50,100],100)
    assert result['p99_ms']==50
    assert result['low_1pct_fps']==10
    assert result['over_1_5_budget_percent']==2
    assert result['over_50ms']==1


def test_warmup_capture_and_minute_boundaries():
    rows=[dict(seconds=t,frame_ms=10,selected=2,scene_lights=4,
               occlusion_updates=1,capture_excluded=0,tick=t*30) for t in range(160)]
    rows[35].update(frame_ms=1000,capture_excluded=1)
    rows[95]['frame_ms']=200  # Real hitch must remain in both overall and per-minute data.
    result=summarize(rows,30,120,100)
    assert result['raw']['frames']==120 and result['pacing']['frames']==119
    assert result['raw']['worst_ms']==1000 and result['pacing']['worst_ms']==200
    assert result['minutes'][0]['worst_ms']==10
    assert result['minutes'][1]['worst_ms']==200
    assert result['measured_span']==119
    assert result['selected_max']==2


def test_missing_frame_samples_are_not_a_pass():
    with pytest.raises(ValueError,match='No frame samples'):
        distribution([],60)
