#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <base.h>
#include <logger.hpp>

#include "gen/config.hpp"


struct context
{
    ::config  config;
    ::logger *logger;
};


#endif // CONTEXT_HPP
