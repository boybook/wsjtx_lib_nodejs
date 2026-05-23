# Idempotent source overlay for Q65 TX/RX support.
# The parent package consumes boybook/wsjtx_lib as a submodule, so these
# targeted replacements keep this binding self-contained without requiring a
# forked submodule URL.

function(wsjtx_replace_once file needle replacement description)
  file(READ "${file}" _content)
  string(FIND "${_content}" "${replacement}" _already)
  if(_already GREATER_EQUAL 0)
    message(STATUS "Q65 patch already applied: ${description}")
    return()
  endif()
  string(FIND "${_content}" "${needle}" _found)
  if(_found LESS 0)
    message(FATAL_ERROR "Q65 patch failed: ${description}; needle not found in ${file}")
  endif()
  string(REPLACE "${needle}" "${replacement}" _content "${_content}")
  file(WRITE "${file}" "${_content}")
  message(STATUS "Applied Q65 patch: ${description}")
endfunction()

set(_WSJTX_LIB_DIR "${CMAKE_SOURCE_DIR}/wsjtx_lib")

# ---- wsjtx_encode.h --------------------------------------------------------
set(_encode_h "${_WSJTX_LIB_DIR}/wsjtx_encode.h")
wsjtx_replace_once(
  "${_encode_h}"
  "\t  std::vector<float> encode_ft4(wsjtxMode mode, int frequency, std::string message, std::string &msgsent, int sampleRate);\n\t  std::vector<float> encode_wspr(wsjtxMode mode, int frequency, std::string message, std::string &msgsent);"
  "\t  std::vector<float> encode_ft4(wsjtxMode mode, int frequency, std::string message, std::string &msgsent, int sampleRate);\n\t  std::vector<float> encode_q65(wsjtxMode mode, int frequency, std::string message, std::string &msgsent, int sampleRate, int q65Period, int q65Submode);\n\t  std::vector<float> encode_wspr(wsjtxMode mode, int frequency, std::string message, std::string &msgsent);"
  "declare parameterized wsjtx_encode::encode_q65")

# ---- wsjtx_encode.cpp ------------------------------------------------------
set(_encode_cpp "${_WSJTX_LIB_DIR}/wsjtx_encode.cpp")
set(_q65_impl [=[
std::vector<float> wsjtx_encode::encode_q65(wsjtxMode mode, int frequency, std::string message, std::string &msgsent, int sampleRate, int q65Period, int q65Submode)
{
	std::vector<float> signal;

	int ichk = 0;
	int i3 = -1;
	int n3 = -1;

	std::memset(msg, 0, 38);
	std::memset(sendmsg, 0, 38);
	std::memset(itone, 0, sizeof(itone));
	std::copy_n(message.c_str(), std::min<size_t>(message.size(), 37), msg);

	genq65_(msg, &ichk, sendmsg, const_cast<int *>(itone), &i3, &n3, 37, 37);
	sendmsg[37] = '\0';
	msgsent = std::string(sendmsg);

	int nsym = 85;
	int ntrperiod = q65Period;
	if (ntrperiod != 30 && ntrperiod != 60 && ntrperiod != 120 && ntrperiod != 300) ntrperiod = 60;
	int nsubmode = q65Submode;
	if (nsubmode < 0 || nsubmode > 4) nsubmode = 0;
	int hmod = 1 << nsubmode;

	int baseNsps = 7200;
	if (ntrperiod == 30) baseNsps = 3600;
	else if (ntrperiod == 60) baseNsps = 7200;
	else if (ntrperiod == 120) baseNsps = 16000;
	else if (ntrperiod == 300) baseNsps = 41472;

	int nsps = (sampleRate / 12000) * baseNsps;
	if (nsps <= 0) nsps = baseNsps;
	float fsample = static_cast<float>(sampleRate);
	float f0 = static_cast<float>(frequency);
	int icmplx = 0;
	int nwave = nsym * nsps;

	signal.assign(static_cast<size_t>(ntrperiod) * static_cast<size_t>(sampleRate), 0.0f);
	std::vector<float> cwave(static_cast<size_t>(nwave) * 2U, 0.0f);
	genwave_(const_cast<int *>(itone), &nsym, &nsps, &nwave,
			 &fsample, &hmod, &f0, &icmplx, cwave.data(), signal.data());
	return signal;
}

]=])
wsjtx_replace_once(
  "${_encode_cpp}"
  "std::vector<float> wsjtx_encode::encode_wspr(wsjtxMode mode, int frequency, std::string message, std::string &msgsent)"
  "${_q65_impl}std::vector<float> wsjtx_encode::encode_wspr(wsjtxMode mode, int frequency, std::string message, std::string &msgsent)"
  "implement parameterized wsjtx_encode::encode_q65")

# ---- wsjtx_lib.h -----------------------------------------------------------
set(_lib_h "${_WSJTX_LIB_DIR}/wsjtx_lib.h")
wsjtx_replace_once(
  "${_lib_h}"
  "\tstd::vector<float> encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend, int sampleRate);"
  "\tstd::vector<float> encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend, int sampleRate, int q65Period = 60, int q65Submode = 0);"
  "extend wsjtx_lib::encode signature for Q65 options")
wsjtx_replace_once(
  "${_lib_h}"
  "\tvoid setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);"
  "\tvoid setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);\n\tvoid setDecodeQ65Controls(int period, int submode, int maxDrift, bool clearAveraging, bool singleDecode, bool averaging);"
  "declare wsjtx_lib Q65 decode controls")
wsjtx_replace_once(
  "${_lib_h}"
  "\tint qso_progress_ = 0;\n\tDataQueue<WsjtxMessage> messageQueue_;"
  "\tint qso_progress_ = 0;\n\tint q65_period_ = 60;\n\tint q65_submode_ = 0;\n\tint q65_max_drift_ = 50;\n\tbool q65_clear_averaging_ = false;\n\tbool q65_single_decode_ = false;\n\tbool q65_averaging_ = false;\n\tDataQueue<WsjtxMessage> messageQueue_;"
  "add wsjtx_lib Q65 decode state")

# ---- wsjtx_lib.cpp ---------------------------------------------------------
set(_lib_cpp "${_WSJTX_LIB_DIR}/wsjtx_lib.cpp")
wsjtx_replace_once(
  "${_lib_cpp}"
  "void wsjtx_lib::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress)\n{\n\tap_decode_ = apDecode;\n\tdecode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;\n\ttx_frequency_ = txFrequency;\n\tqso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;\n}"
  "void wsjtx_lib::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress)\n{\n\tap_decode_ = apDecode;\n\tdecode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;\n\ttx_frequency_ = txFrequency;\n\tqso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;\n}\n\nvoid wsjtx_lib::setDecodeQ65Controls(int period, int submode, int maxDrift, bool clearAveraging, bool singleDecode, bool averaging)\n{\n\tq65_period_ = (period == 30 || period == 60 || period == 120 || period == 300) ? period : 60;\n\tq65_submode_ = (submode >= 0 && submode <= 4) ? submode : 0;\n\tq65_max_drift_ = maxDrift < 0 ? 50 : maxDrift;\n\tq65_clear_averaging_ = clearAveraging;\n\tq65_single_decode_ = singleDecode;\n\tq65_averaging_ = averaging;\n}"
  "implement wsjtx_lib Q65 controls")
wsjtx_replace_once(
  "${_lib_cpp}"
  "\tptr->setDecodeControls(ap_decode_, decode_depth_, tx_frequency_, qso_progress_);"
  "\tptr->setDecodeControls(ap_decode_, decode_depth_, tx_frequency_, qso_progress_);\n\tptr->setDecodeQ65Controls(q65_period_, q65_submode_, q65_max_drift_, q65_clear_averaging_, q65_single_decode_, q65_averaging_);"
  "forward Q65 decode controls")
wsjtx_replace_once(
  "${_lib_cpp}"
  "std::vector<float> wsjtx_lib::encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend, int sampleRate)"
  "std::vector<float> wsjtx_lib::encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend, int sampleRate, int q65Period, int q65Submode)"
  "extend wsjtx_lib::encode implementation signature")
wsjtx_replace_once(
  "${_lib_cpp}"
  "\tcase FT4: {\n\t\tauto ptr = std::make_unique<wsjtx_encode>();\n\t\treturn ptr->encode_ft4(mode, frequency, message, messagesend, sampleRate);\n\t}\n\tdefault: return {};"
  "\tcase FT4: {\n\t\tauto ptr = std::make_unique<wsjtx_encode>();\n\t\treturn ptr->encode_ft4(mode, frequency, message, messagesend, sampleRate);\n\t}\n\tcase Q65: {\n\t\tauto ptr = std::make_unique<wsjtx_encode>();\n\t\treturn ptr->encode_q65(mode, frequency, message, messagesend, sampleRate, q65Period, q65Submode);\n\t}\n\tdefault: return {};"
  "route Q65 encode in wsjtx_lib")

# ---- wsjtx_decode.h --------------------------------------------------------
set(_decode_h "${_WSJTX_LIB_DIR}/wsjtx_decode.h")
wsjtx_replace_once(
  "${_decode_h}"
  "\tvoid setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);"
  "\tvoid setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);\n\tvoid setDecodeQ65Controls(int period, int submode, int maxDrift, bool clearAveraging, bool singleDecode, bool averaging);"
  "declare wstjx_decode Q65 controls")
wsjtx_replace_once(
  "${_decode_h}"
  "\tint qso_progress_ = 0;\n\tstd::string my_call_, my_grid_;"
  "\tint qso_progress_ = 0;\n\tint q65_period_ = 60;\n\tint q65_submode_ = 0;\n\tint q65_max_drift_ = 50;\n\tbool q65_clear_averaging_ = false;\n\tbool q65_single_decode_ = false;\n\tbool q65_averaging_ = false;\n\tstd::string my_call_, my_grid_;"
  "add wstjx_decode Q65 state")

# ---- wsjtx_decode.cpp ------------------------------------------------------
set(_decode_cpp "${_WSJTX_LIB_DIR}/wsjtx_decode.cpp")
wsjtx_replace_once(
  "${_decode_cpp}"
  "#include <ctime>\n#include <time.h>"
  "#include <ctime>\n#include <time.h>\n#include <algorithm>"
  "include <algorithm> for Q65 frame sizing")
wsjtx_replace_once(
  "${_decode_cpp}"
  "void wstjx_decode::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress) {\n\tap_decode_ = apDecode;\n\tdecode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;\n\ttx_frequency_ = txFrequency;\n\tqso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;\n}"
  "void wstjx_decode::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress) {\n\tap_decode_ = apDecode;\n\tdecode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;\n\ttx_frequency_ = txFrequency;\n\tqso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;\n}\nvoid wstjx_decode::setDecodeQ65Controls(int period, int submode, int maxDrift, bool clearAveraging, bool singleDecode, bool averaging) {\n\tq65_period_ = (period == 30 || period == 60 || period == 120 || period == 300) ? period : 60;\n\tq65_submode_ = (submode >= 0 && submode <= 4) ? submode : 0;\n\tq65_max_drift_ = maxDrift < 0 ? 50 : maxDrift;\n\tq65_clear_averaging_ = clearAveraging;\n\tq65_single_decode_ = singleDecode;\n\tq65_averaging_ = averaging;\n}"
  "implement wstjx_decode Q65 controls")
wsjtx_replace_once(
  "${_decode_cpp}"
  "\tsamplebuffer.push(std::move(audiosamples));"
  "\tsamplebuffer.push(WsjTxVector(audiosamples));"
  "preserve Float32 decode samples before decoder copy")
wsjtx_replace_once(
  "${_decode_cpp}"
  "\tfor (size_t i = 0; i < audiosamples.size(); i++)\n\t\tdec_data.d2[i] = (short int)(audiosamples[i] * 32768.0f);"
  "\tfor (size_t i = 0; i < audiosamples.size(); i++) {\n\t\tfloat sample = std::clamp(audiosamples[i], -1.0f, 0.9999695f);\n\t\tdec_data.d2[i] = static_cast<short int>(sample * 32768.0f);\n\t}"
  "clamp Float32 samples before Int16 decoder conversion")
set(_q65_switch [=[
	case FT8: params.nmode = 8; break;
	case FT4: params.nmode = 5; break;
	case Q65:
		params.nmode = 66;
		params.ntrperiod = q65_period_;
		params.kin = std::min(static_cast<int>(audiosamples.size()), q65_period_ * 12000);
		params.nzhsym = 85;
		params.nsubmode = q65_submode_;
		params.ntxmode = 66;
		params.max_drift = q65_max_drift_;
		params.nclearave = q65_clear_averaging_;
		if (q65_single_decode_) params.nexp_decode |= 32;
		if (q65_averaging_) params.ndepth |= 16;
		break;
	default: return;]=])
wsjtx_replace_once(
  "${_decode_cpp}"
  "\tcase FT8: params.nmode = 8; break;\n\tcase FT4: params.nmode = 5; break;\n\tdefault: return;"
  "${_q65_switch}"
  "select parameterized Q65 decoder mode")

# ---- lib/decode_callbacks.f90 ---------------------------------------------
set(_callbacks_f90 "${_WSJTX_LIB_DIR}/lib/decode_callbacks.f90")
wsjtx_replace_once(
  "${_callbacks_f90}"
  "    endif\n    call flush(6)\n\n    select type(ctx => this)\n    type is (counting_q65_decoder)"
  "    endif\n    call wsjtx_decoded(nutc,nsnr,dt,nint(freq),decoded)\n    call flush(6)\n\n    select type(ctx => this)\n    type is (counting_q65_decoder)"
  "forward Q65 decode callback into C queue")
