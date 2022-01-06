#ifdef BOOST_ENABLE_ASSERT_HANDLER

#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/format.hpp>
#include <mujincontrollerclient/mujinexceptions.h>

// Derived from https://gcc.gnu.org/wiki/Visibility
#if !(defined _WIN32 || defined __CYGWIN__) && __GNUC__ >= 4
  #define HIDDEN  __attribute__ ((visibility ("hidden")))
#else
  #define HIDDEN
#endif

/// Modifications controlling %boost library behavior.
namespace boost
{
HIDDEN void assertion_failed(char const * expr, char const * function, char const * file, long line)
{
    throw mujinclient::MujinException((boost::format("[%s:%d] -> %s, expr: %s")%file%line%function%expr).str(),mujinclient::MEC_Assert);
}

#if BOOST_VERSION>104600
HIDDEN void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line)
{
    throw mujinclient::MujinException((boost::format("[%s:%d] -> %s, expr: %s, msg: %s")%file%line%function%expr%msg).str(),mujinclient::MEC_Assert);
}
#endif
} // namespace boost

#endif
