#include "../include/playout.h"
#include "../include/eval.h"
#include "../include/utils.h"
#include "../include/params.h"
#include "../data/canonical_sequences.h"

using namespace std;

/** Selects the highest value lock placement using the fast eval function. */
SimState pickLockPlacement(GameState gameState,
                           EvalContext evalContext,
                           FastEvalWeights evalWeights,
                           OUT vector<SimState> &lockPlacements) {
  float bestSoFar = evalWeights.deathCoef;
  SimState bestPlacement = {};
  for (auto lockPlacement : lockPlacements) {
    GameState newState = advanceGameState(gameState, lockPlacement, evalContext);
    float evalScore = fastEval(gameState, newState, lockPlacement, evalContext, evalWeights);
    if (evalScore > bestSoFar) {
      bestSoFar = evalScore;
      bestPlacement = lockPlacement;
    }
  }
  maybePrint("\nBest placement: %d %d\n", bestPlacement.rotationIndex, bestPlacement.x - SPAWN_X);
  return bestPlacement;
}


float getPlayoutScore(GameState gameState, int numPlayouts){
  float totalScore = 0;
  for (int i = 0; i < numPlayouts; i++) {
    // Do one playout
    const int *pieceSequence = canonicalPieceSequences + i * 10; // Index into the mega array of piece sequences;
    float playoutScore = playSequence(gameState, pieceSequence);
    totalScore += playoutScore;
  }
  // printf("Playout set ended with totalScore %f\n", totalScore);
  return totalScore / numPlayouts;
}


/**
 * Plays out a starting state 10 moves into the future.
 * @returns the total value of the playout (intermediate rewards + eval of the final board)
 */
float playSequence(GameState gameState, const int pieceSequence[10]) {
  float totalReward = 0;
  for (int i = 0; i < 10; i++) {
    // Figure out modes and eval context
    EvalContext evalContext = getEvalContext(gameState);
    FastEvalWeights weights = getWeights(evalContext.aiMode);

    // Get the lock placements
    std::vector<SimState> lockPlacements;
    Piece piece = PIECE_LIST[pieceSequence[i]];
    moveSearch(gameState, piece, evalContext.inputFrameTimeline, lockPlacements);

    if (lockPlacements.size() == 0) {
      return weights.deathCoef;
    }

    // Pick the best placement
    SimState bestMove = pickLockPlacement(gameState, evalContext, weights, lockPlacements);

    // On the last move, do a final evaluation
    if (i == 9) {
      GameState nextState = advanceGameState(gameState, bestMove, evalContext);
      float evalScore = fastEval(gameState, nextState, bestMove, evalContext, weights);
      if (PLAYOUT_LOGGING_ENABLED) {
        gameState = nextState;
        printBoard(gameState.board);
        printf("Best placement: %c %d, %d\n\n", bestMove.piece.id, bestMove.rotationIndex, bestMove.x - SPAWN_X);
        printf("Cumulative reward: %01f\n", totalReward);
        printf("Final eval score: %01f\n", evalScore);
      }
      return totalReward + evalScore;
    }

    // Otherwise, update the state to keep playing
    int oldLines = gameState.lines;
    gameState = advanceGameState(gameState, bestMove, evalContext);
    FastEvalWeights rewardWeights = evalContext.aiMode == DIG ? getWeights(STANDARD) : weights; // When the AI is digging, still deduct from the overall value of the sequence at standard levels
    totalReward += getLineClearFactor(gameState.lines - oldLines, rewardWeights);
    if (PLAYOUT_LOGGING_ENABLED) {
      printBoard(gameState.board);
      printf("Best placement: %c %d, %d\n\n", bestMove.piece.id, bestMove.rotationIndex, bestMove.x - SPAWN_X);
    }
  }
  return -1; // Doesn't reach here, always returns from i == 9 case
}
