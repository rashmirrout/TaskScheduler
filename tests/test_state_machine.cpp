#include <gtest/gtest.h>
#include "task_base.h"
#include <atomic>
#include <vector>

using namespace task_scheduler;

// Mock task for testing state machine behavior
class MockTask : public TaskBase {
public:
    MockTask(const std::string& name, int intervalMs)
        : TaskBase(name, intervalMs, 10, 0, 10, 0)  // tolerance=10, no repeat
        , planSignal_(false)
        , planAct_(false)
        , signalActivations_(0)
        , signalDeactivations_(0)
        , actActivations_(0)
        , actDeactivations_(0)
    {}

    PlanResult plan() override {
        return {planSignal_.load(), planAct_.load()};
    }

    void signal(bool doSignal) override {
        if (doSignal) {
            signalActivations_++;
        } else {
            signalDeactivations_++;
        }
    }

    void act(bool doAct) override {
        if (doAct) {
            actActivations_++;
        } else {
            actDeactivations_++;
        }
    }

    // Test control methods
    void setPlan(bool wantSignal, bool wantAct) {
        planSignal_ = wantSignal;
        planAct_ = wantAct;
    }

    int getSignalActivations() const { return signalActivations_.load(); }
    int getSignalDeactivations() const { return signalDeactivations_.load(); }
    int getActActivations() const { return actActivations_.load(); }
    int getActDeactivations() const { return actDeactivations_.load(); }

    void resetCounters() {
        signalActivations_ = 0;
        signalDeactivations_ = 0;
        actActivations_ = 0;
        actDeactivations_ = 0;
    }

private:
    std::atomic<bool> planSignal_;
    std::atomic<bool> planAct_;
    std::atomic<int> signalActivations_;
    std::atomic<int> signalDeactivations_;
    std::atomic<int> actActivations_;
    std::atomic<int> actDeactivations_;
};

class StateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        task = std::make_shared<MockTask>("TestTask", 100);
    }

    std::shared_ptr<MockTask> task;
};

TEST_F(StateMachineTest, NoiseFiltering) {
    // Set tolerance to 10, so signal should activate after 10 consecutive runs
    task->setPlan(true, false);

    // Run 9 times - should NOT activate (below tolerance)
    for (int i = 0; i < 9; ++i) {
        task->run();
    }

    EXPECT_EQ(task->getSignalActivations(), 0);
    EXPECT_EQ(task->getSignalDeactivations(), 0);
}

TEST_F(StateMachineTest, ActivationAtTolerance) {
    task->setPlan(true, false);

    // Run 10 times - should activate on 10th run
    for (int i = 0; i < 10; ++i) {
        task->run();
    }

    EXPECT_EQ(task->getSignalActivations(), 1);
    EXPECT_EQ(task->getSignalDeactivations(), 0);
}

TEST_F(StateMachineTest, ImmediateDeactivation) {
    task->setPlan(true, false);

    // Activate signal
    for (int i = 0; i < 10; ++i) {
        task->run();
    }
    EXPECT_EQ(task->getSignalActivations(), 1);

    // Change plan to false - should deactivate immediately
    task->setPlan(false, false);
    task->run();

    EXPECT_EQ(task->getSignalDeactivations(), 1);
}

TEST_F(StateMachineTest, GlitchRejection) {
    task->setPlan(true, false);

    // Run 5 times
    for (int i = 0; i < 5; ++i) {
        task->run();
    }

    // Glitch - one false reading
    task->setPlan(false, false);
    task->run();

    // Back to true
    task->setPlan(true, false);
    for (int i = 0; i < 5; ++i) {
        task->run();
    }

    // Should still not have activated (counter reset on glitch)
    EXPECT_EQ(task->getSignalActivations(), 0);
}

TEST_F(StateMachineTest, RepeatHeartbeat) {
    // Create task with repeat enabled
    auto repeatTask = std::make_shared<MockTask>("RepeatTask", 100);
    repeatTask->updateConfig(100, 5, 3, true, 5, 0, true);  // tolerance=5, repeat=3
    repeatTask->setPlan(true, false);

    // First 5 runs - activate on 5th
    for (int i = 0; i < 5; ++i) {
        repeatTask->run();
    }
    EXPECT_EQ(repeatTask->getSignalActivations(), 1);

    // Next 3 runs - should re-fire on 3rd (heartbeat)
    for (int i = 0; i < 3; ++i) {
        repeatTask->run();
    }
    EXPECT_EQ(repeatTask->getSignalActivations(), 2);

    // Another 3 runs - another heartbeat
    for (int i = 0; i < 3; ++i) {
        repeatTask->run();
    }
    EXPECT_EQ(repeatTask->getSignalActivations(), 3);
}

TEST_F(StateMachineTest, GateClosed) {
    task->setPlan(true, false);

    // Run 10 times with gate open
    for (int i = 0; i < 10; ++i) {
        task->run();
    }
    EXPECT_EQ(task->getSignalActivations(), 1);

    // Continue running but close the gate
    task->updateConfig(100, 10, 0, false, 10, 0, true);  // allowSignal=false
    task->run();

    // Should deactivate when gate closes
    EXPECT_EQ(task->getSignalDeactivations(), 1);
}

TEST_F(StateMachineTest, GatePreventActivation) {
    // Start with gate closed
    task->updateConfig(100, 10, 0, false, 10, 0, true);  // allowSignal=false
    task->setPlan(true, false);

    // Run 20 times
    for (int i = 0; i < 20; ++i) {
        task->run();
    }

    // Should never activate
    EXPECT_EQ(task->getSignalActivations(), 0);
}

TEST_F(StateMachineTest, IndependentChannels) {
    task->setPlan(true, true);

    // Run to activate both channels
    for (int i = 0; i < 10; ++i) {
        task->run();
    }

    EXPECT_EQ(task->getSignalActivations(), 1);
    EXPECT_EQ(task->getActActivations(), 1);

    // Deactivate only signal channel
    task->setPlan(false, true);
    task->run();

    EXPECT_EQ(task->getSignalDeactivations(), 1);
    EXPECT_EQ(task->getActDeactivations(), 0);  // Act should still be active
}

TEST_F(StateMachineTest, SingleShotBehavior) {
    // No repeat configured (sigRepeat=0)
    task->setPlan(true, false);

    // Activate
    for (int i = 0; i < 10; ++i) {
        task->run();
    }
    EXPECT_EQ(task->getSignalActivations(), 1);

    // Continue running - should not re-fire
    for (int i = 0; i < 20; ++i) {
        task->run();
    }
    EXPECT_EQ(task->getSignalActivations(), 1);  // Still only 1
}
