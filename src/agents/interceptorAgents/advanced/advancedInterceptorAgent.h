#ifndef ADVANCED_INTERCEPTOR_AGENT_H
#define ADVANCED_INTERCEPTOR_AGENT_H

#include "interceptorAgent.h"

/**
 * AdvancedInterceptorAgent
 *
 * Strongest interceptor implementation and has cost 3.
 *
 * Kill probabilities:
 *   - inner 50%: 99%
 *   - 50%-70%:  90%
 *   - 70%-100%: 80%
 */
struct AdvancedInterceptorAgent : public InterceptorAgent {
    AdvancedInterceptorAgent(
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

#endif // ADVANCED_INTERCEPTOR_AGENT_H