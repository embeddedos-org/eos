# Chapter 32: eStocks — Algorithmic Trading System

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 32.1 Introduction

**eStocks** is a comprehensive algorithmic trading system with **15 strategies**,
**7 data sources**, **7-layer risk management**, and full production safety
controls. It includes 288+ tests, thread-safe state persistence, and
crash-recoverable operation.

---

## 32.2 Quick Start

```bash
# Setup (installs deps, validates system, tests connectivity)
python setup_trading.py

# Paper trade with real Yahoo Finance data (no broker needed)
python paper_trader.py --symbols AAPL,MSFT,GOOGL --strategy meta_ensemble

# Scan 15 stocks with all strategies
python paper_trader.py --scan-universe

# Run tests
python -m pytest tests/ -v
```

---

## 32.3 The 15 Trading Strategies

| #  | Strategy         | Based On                              |
|----|------------------|---------------------------------------|
| 1  | Trend Following  | EMA crossover + ADX + trailing stop   |
| 2  | Breakout         | Donchian channel breakout             |
| 3  | Mean Reversion   | RSI + Bollinger Bands                 |
| 4  | Factor           | 12-1 month momentum long/short        |
| 5  | Darvas Box       | Darvas box breakout                   |
| 6  | Triple Screen    | Elder triple screen system            |
| 7  | CAN SLIM         | O'Neil CAN SLIM 7-criteria           |
| 8  | Value            | Graham fundamental value              |
| 9  | ML (LSTM)        | LSTM deep learning                    |
| 10 | RL (PPO)         | PPO reinforcement learning            |
| 11 | Self-Learning    | Adaptive ML ensemble                  |
| 12 | Sentiment        | News sentiment + technicals           |
| 13 | Earnings         | Earnings calendar trading             |
| 14 | Sector Rotation  | Sector ETF momentum                   |
| 15 | Meta Ensemble    | All sources combined                  |

All 15 strategies use all 7 data sources: Price, Volume, Fundamentals,
News, Earnings, ML predictions, and Market Regime detection.

---

## 32.4 Seven-Layer Risk Management

```
Layer 7: PORTFOLIO HEAT    Max 20% equity at risk
Layer 6: POSITION CAP      Max 25% equity / 10K shares per position
Layer 5: CIRCUIT BREAKER    10% drawdown -> 24h pause
Layer 4: MONTHLY CAP       Elder 6% monthly loss limit
Layer 3: DAILY LIMIT       $5,000/day hard stop
Layer 2: COOLDOWN          30-min pause after 3 consecutive losses
Layer 1: PER-TRADE RISK    2% of equity per trade
```

---

## 32.5 Production Safety Controls

| Control                | Threshold              |
|------------------------|------------------------|
| Fat-finger protection  | 10K shares max         |
| Price deviation        | +/-10% limit           |
| Short limits           | 5 positions / 30%      |
| Liquidity filter       | 50K min daily volume   |
| Market hours           | Enforced               |
| State persistence      | SQLite WAL (thread-safe)|

---

## 32.6 Platform Support

| Platform               | Language      | Broker                 |
|------------------------|---------------|------------------------|
| TradingView            | Pine Script   | IB, TradeStation       |
| thinkorswim            | thinkScript   | Charles Schwab         |
| Interactive Brokers    | Python (TWS)  | Interactive Brokers    |
| TradeStation           | EasyLanguage  | TradeStation           |

---

## 32.7 Architecture

```
stocks_plugin/
├── shared/
│   ├── risk_manager.py           # 7-layer risk engine
│   ├── strategy_enricher.py      # Multi-source data enrichment
│   ├── trade_journal.py          # Psychology/discipline journal
│   ├── data/public_data_fetcher.py # OHLCV, fundamentals, news
│   ├── indicators/               # 35+ indicators, 14 patterns
│   ├── backtesting/              # Multi-asset backtester
│   └── ml/                       # Sentiment, regime, LSTM, RL
├── strategies/examples/          # 15 registered strategies
├── tests/                        # 288+ tests
├── setup_trading.py              # One-command setup
└── paper_trader.py               # Paper trading simulator
```

---

## 32.8 Backtesting

The backtesting engine supports multi-asset testing with R-multiples
and System Quality Number (SQN) analysis:

```bash
python -m stocks_plugin.shared.backtesting.runner \
  --strategy trend_following \
  --symbols AAPL,MSFT,GOOGL,AMZN \
  --start 2020-01-01 \
  --end 2025-12-31 \
  --initial-capital 100000
```

Output includes:
- Total return and CAGR
- Maximum drawdown
- Sharpe ratio and Sortino ratio
- Win rate and R-multiple distribution
- SQN score

---

## 32.9 ML and AI Strategies

### LSTM Deep Learning (Strategy 9)

Uses Long Short-Term Memory networks trained on historical price data
to predict next-day price movements.

### PPO Reinforcement Learning (Strategy 10)

Uses Proximal Policy Optimization to learn optimal trading policies
through simulated market interaction.

### Self-Learning Ensemble (Strategy 11)

An adaptive system that continuously retrains its model ensemble based
on recent market conditions.

### Sentiment Analysis (Strategy 12)

Analyzes news headlines and social media sentiment using NLP to generate
trading signals.

---

## 32.10 CI/CD

- **Tests:** Python 3.10/3.11/3.12 matrix
- **Security:** Bandit scan, hardcoded secrets check
- **Strategy validation:** Verifies all 15 strategies register correctly
- **Release:** Tag `v*` triggers automated GitHub release

---

## 32.11 Summary

eStocks provides a production-grade algorithmic trading system with
comprehensive risk management and multiple strategy paradigms.

**Key takeaways:**

- 15 trading strategies spanning classical and ML approaches
- 7-layer risk management with production safety controls
- Paper trading simulator requiring no broker account
- Multi-platform: TradingView, thinkorswim, IB, TradeStation
- 288+ tests with thread-safe crash recovery
- Backtesting with R-multiple and SQN analysis

---

*Next: Chapter 33 — Cross-Compilation*
