#ifndef BASIC_INTERCEPTOR_AGENT_H
#define BASIC_INTERCEPTOR_AGENT_H

#include "interceptorAgent.h"

/**
 * BasicInterceptorAgent
 *
 * Uses InterceptorAgent::killProbability() unchanged and has cost 1.
 */
struct BasicInterceptorAgent : public InterceptorAgent {
    BasicInterceptorAgent(
        int id,
        int row,
        int col,
        double killRadius
    );
};

#endif // BASIC_INTERCEPTOR_AGENT_H