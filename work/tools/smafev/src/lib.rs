// smafev_convert(data, len, &out_len): SMAF 하나를 이벤트 레코드로 바꿔 malloc 버퍼로 돌려준다. 해제는 smafev_free.
// 레코드 형식(리틀 엔디언)은 크로노스 윙 game.ev의 항목과 같다: 이벤트 수 u32, [시각(ms) u32, 종류 u8, ...]
// 종류 0 = MIDI(길이 u32, 바이트), 1 = 파형(채널 u8, 샘플링 u32, 샘플 수 u32, i16...)
use smaf_player::{parse_smaf, SmafEvent};

fn convert(raw: &[u8]) -> Vec<u8> {
    let events = parse_smaf(raw);
    let mut out = Vec::new();
    out.extend_from_slice(&(events.len() as u32).to_le_bytes());
    for (time, ev) in events {
        out.extend_from_slice(&(time as u32).to_le_bytes());
        let midi: Vec<u8> = match ev {
            SmafEvent::Wave { channel, sampling_rate, data } => {
                out.push(1);
                out.push(channel);
                out.extend_from_slice(&sampling_rate.to_le_bytes());
                out.extend_from_slice(&(data.len() as u32).to_le_bytes());
                for s in data {
                    out.extend_from_slice(&s.to_le_bytes());
                }
                continue;
            }
            SmafEvent::MidiNoteOn { channel, note, velocity } => vec![0x90 | channel, note, velocity],
            SmafEvent::MidiNoteOff { channel, note, velocity } => vec![0x80 | channel, note, velocity],
            SmafEvent::MidiProgramChange { channel, program } => vec![0xc0 | channel, program],
            SmafEvent::MidiControlChange { channel, control, value } => vec![0xb0 | channel, control, value],
            SmafEvent::MidiPitchBend { channel, value } => vec![0xe0 | channel, (value & 0x7f) as u8, ((value >> 7) & 0x7f) as u8],
            SmafEvent::MidiSysEx(data) => data,
            SmafEvent::End => vec![],
        };
        out.push(0);
        out.extend_from_slice(&(midi.len() as u32).to_le_bytes());
        out.extend_from_slice(&midi);
    }
    out
}

#[no_mangle]
pub unsafe extern "C" fn smafev_convert(data: *const u8, len: usize, out_len: *mut usize) -> *mut u8 {
    let raw = std::slice::from_raw_parts(data, len);
    let out = match std::panic::catch_unwind(|| convert(raw)) {
        Ok(v) => v.into_boxed_slice(),
        Err(_) => return std::ptr::null_mut(),
    };
    *out_len = out.len();
    Box::into_raw(out) as *mut u8
}

#[no_mangle]
pub unsafe extern "C" fn smafev_free(p: *mut u8, len: usize) {
    if !p.is_null() {
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(p, len)));
    }
}

// 웹 빌드(wasm32-unknown-unknown)에서 페이지가 입력과 결과 길이 칸을 이 모듈의 메모리에 잡을 때 쓴다(host/audio.c의 EM_JS).
#[no_mangle]
pub extern "C" fn smafev_alloc(len: usize) -> *mut u8 {
    Box::into_raw(vec![0u8; len.max(1)].into_boxed_slice()) as *mut u8
}

#[no_mangle]
pub unsafe extern "C" fn smafev_dealloc(p: *mut u8, len: usize) {
    smafev_free(p, len.max(1))
}
