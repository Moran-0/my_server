#pragma once

class NoCopy {
  public:
    NoCopy() = default;
    ~NoCopy() = default;
    NoCopy(NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

class NoMove {
  public:
    NoMove() = default;
    ~NoMove() = default;
    NoMove(NoMove&&) = delete;
    NoMove& operator=(NoMove&&) = delete;
};