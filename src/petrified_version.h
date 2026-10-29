/*
  Petrified-Stockfish version

  The source tree carries no release number. A release build passes its
  version on the compiler command line as PETRIFIED_VERSION=MAJOR.MINOR.PATCH
  (the Makefile, setup.py and the WebAssembly builds all take it from an
  PETRIFIED_VERSION variable). Every other build is a development build and names
  its commit instead: dev-YYYYMMDD-<sha>, see engine_version_info() in misc.cpp.
*/

#ifndef PETRIFIED_VERSION_H_INCLUDED
#define PETRIFIED_VERSION_H_INCLUDED

#include <cstddef>
#include <string_view>

#define PETRIFIED_VERSION_STRINGIFY2(x) #x
#define PETRIFIED_VERSION_STRINGIFY(x) PETRIFIED_VERSION_STRINGIFY2(x)

namespace Stockfish {

#ifdef PETRIFIED_VERSION
inline constexpr std::string_view PetrifiedVersionString = PETRIFIED_VERSION_STRINGIFY(PETRIFIED_VERSION);
#else
inline constexpr std::string_view PetrifiedVersionString = "dev";
#endif

inline constexpr bool PetrifiedIsRelease = PetrifiedVersionString != "dev";

namespace PetrifiedVersionDetail {

struct ReleaseNumber {
    bool valid;
    int  parts[3];
};

// Splits MAJOR.MINOR.PATCH (decimal, no leading zeros) into its components.
constexpr ReleaseNumber parse(std::string_view text) {
    constexpr ReleaseNumber invalid{false, {0, 0, 0}};
    int                     parts[3] = {0, 0, 0};
    int                     part     = 0;
    std::size_t             digits   = 0;

    for (char c : text)
    {
        if (c == '.')
        {
            if (digits == 0 || part == 2)
                return invalid;
            ++part;
            digits = 0;
        }
        else if (c >= '0' && c <= '9')
        {
            if ((digits == 1 && parts[part] == 0) || digits == 9)
                return invalid;
            parts[part] = parts[part] * 10 + (c - '0');
            ++digits;
        }
        else
            return invalid;
    }

    if (part != 2 || digits == 0)
        return invalid;

    return {true, {parts[0], parts[1], parts[2]}};
}

inline constexpr ReleaseNumber Release =
  PetrifiedIsRelease ? parse(PetrifiedVersionString) : ReleaseNumber{true, {0, 0, 0}};

static_assert(Release.valid, "PETRIFIED_VERSION must be MAJOR.MINOR.PATCH");

}  // namespace PetrifiedVersionDetail

// Components of the release version; all zero in development builds.
inline constexpr int PetrifiedVersionMajor = PetrifiedVersionDetail::Release.parts[0];
inline constexpr int PetrifiedVersionMinor = PetrifiedVersionDetail::Release.parts[1];
inline constexpr int PetrifiedVersionPatch = PetrifiedVersionDetail::Release.parts[2];

}  // namespace Stockfish

#endif  // PETRIFIED_VERSION_H_INCLUDED
