#include "cputimer.h"

#include <util/system/compat.h>
#include <util/system/defaults.h>
#include <util/system/hp_timer.h>
#include <util/string/util.h>
#include <util/stream/output.h>
#include <util/generic/singleton.h>

#if defined(_unix_)
#   include <unistd.h>
#   include <sched.h>
#   include <sys/types.h>
#   include <sys/resource.h>
#   include <sys/param.h>
    #if !defined(_cygwin_)
    #   include <sys/sysctl.h>
    // #   include <sys/user.h>
    #endif
#else
#if defined(_win_)
#include <util/system/winint.h>
#endif
#endif

TTimer::TTimer(const TStringBuf& message) {
    static const int SMALL_DURATION_CHAR_LENGTH = 9; // strlen("0.123456s")
    Message_.reserve(message.length() + SMALL_DURATION_CHAR_LENGTH + 1); // +"\n"
    Message_ << message;
    // Do not measure the allocations above.
    Start_ = TInstant::Now();
}

TTimer::~TTimer() {
    const TDuration duration = TInstant::Now() - Start_;
    Message_ << duration << "\n";
    Cerr << Message_.Str();
}

static ui64 ManuallySetCyclesPerSecond = 0;

static ui64 GetCyclesPerSecond() {
    if (ManuallySetCyclesPerSecond != 0)
        return ManuallySetCyclesPerSecond;
    else
        return NHPTimer::GetClockRate();
}

void SetCyclesPerSecond(ui64 cycles) {
    ManuallySetCyclesPerSecond = cycles;
}

ui64 GetCyclesPerMillisecond() {
    return GetCyclesPerSecond() / 1000;
}

TDuration CyclesToDuration(ui64 cycles) {
    return TDuration::MicroSeconds(cycles * 1000000 / GetCyclesPerSecond());
}

ui64 DurationToCycles(TDuration duration) {
    return duration.MicroSeconds() * GetCyclesPerSecond() / 1000000;
}

TPrecisionTimer::TPrecisionTimer(const char* message)
    : Start(GetCycleCount())
    , Message(message)
{
}

TPrecisionTimer::~TPrecisionTimer() {
    Cout << Message << ": " << (double)(GetCycleCount() - Start) << Endl;
}

Stroka FormatCycles(ui64 cycles) {
    ui64 milliseconds = cycles / GetCyclesPerMillisecond();
    ui32 ms = ui32(milliseconds % 1000);
    milliseconds /= 1000;
    ui32 secs = ui32(milliseconds % 60);
    milliseconds /= 60;
    ui32 mins = ui32(milliseconds);
    Stroka result;
    sprintf(result, "%" PRIu32 " m %.2" PRIu32 " s %.3" PRIu32 " ms", mins, secs, ms);
    return result;
}

TFormattedPrecisionTimer::TFormattedPrecisionTimer(const char* message, TOutputStream* out)
    : Message(message)
    , Out(out)
{
    Start = GetCycleCount();
}

TFormattedPrecisionTimer::~TFormattedPrecisionTimer() {
    const ui64 end = GetCycleCount();
    const ui64 diff = end - Start;

    *Out << Message << ": " << diff << " ticks " << FormatCycles(diff) << Endl;
}

TFuncTimer::TFuncTimer(const char* func)
    : Start_(TInstant::Now())
    , Func_(func)
{
    Cerr << "enter " << Func_ << Endl;
}

TFuncTimer::~TFuncTimer() throw () {
    Cerr << "leave " << Func_ << " -> " << (TInstant::Now() - Start_) << Endl;
}

TTimeLogger::TTimeLogger(const Stroka& message, bool verbose)
    : Message(message)
    , Verbose(verbose)
    , OK(false)
    , Begin(time(0))
    , BeginCycles(GetCycleCount())
{
    if (Verbose) {
        fprintf(stderr, "=========================================================\n");
        fprintf(stderr, "%s started: %.24s (%lu) (%d)\n", ~Message, ctime(&Begin), (unsigned long)Begin, (int)getpid());
    }
}

double TTimeLogger::ElapsedTime() const {
    return time(0) - Begin;
}

void TTimeLogger::SetOK() {
    OK = true;
}

TTimeLogger::~TTimeLogger() {
    time_t tim = time(0);
    ui64 endCycles = GetCycleCount();
    if (Verbose) {
        const char* prefix = (OK) ? "" : "!";
        fprintf(stderr, "%s%s ended: %.24s (%lu) (%d) (took %lus = %s)\n",
                prefix, ~Message, ctime(&tim), (unsigned long)tim, (int)getpid(),
                (unsigned long)tim - (unsigned long)Begin, ~FormatCycles(endCycles - BeginCycles));
        fprintf(stderr, "%s=========================================================\n", prefix);
    }
}
