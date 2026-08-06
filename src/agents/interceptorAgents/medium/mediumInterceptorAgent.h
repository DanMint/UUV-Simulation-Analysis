#ifndef MEDIUM_INTERCEPTOR_AGENT_H
#define MEDIUM_INTERCEPTOR_AGENT_H

#include "interceptorAgent.h"

/**
 * MediumInterceptorAgent
 *
 * Stronger than the basic interceptor and has cost 2.
 *
 * Kill probabilities:
 *   - inner 50%: 95%
 *   - 50%-70%:  75%
 *   - 70%-100%: 65%
 */
struct MediumInterceptorAgent : public InterceptorAgent {
    MediumInterceptorAgent(
        int id,
        int row,
        int col,
        double killRadius
    );

    double killProbability(
        int checkRow,
        int checkCol
    ) const override;
};

#endif // MEDIUM_INTERCEPTOR_AGENT_H