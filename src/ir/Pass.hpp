#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/assert.hpp"
#include "util/UniqueAny.hpp"

namespace ir {
class Module;
class Function;

class Pass;
class ImmutablePass;

class PassManager {
    void establishManager();

   public:
    template <typename... Args>
    PassManager(Args... args) {
        static_assert(
            (std::is_constructible_v<std::unique_ptr<Pass>, Args&&> && ...),
            "All arguments to PassManager must be std::unique_ptr<Pass>");
        (passes.push_back(std::unique_ptr<Pass>{std::move(args)}), ...);
        establishManager();
    }

    void addPass(std::unique_ptr<Pass> pass) {
        passes.push_back(std::move(pass));
    }

    void run(ir::Module& module);

    bool empty() const { return passes.empty(); }

    template <typename AnalysisT, typename... ArgsT>
    void setAnalysis(ArgsT&&... args) {
        analysis[std::type_index(typeid(AnalysisT))].emplace<AnalysisT>(
            std::forward<ArgsT>(args)...);
    }

    template <typename AnalysisT>
    AnalysisT& getAnalysis() {
        auto it = analysis.find(std::type_index(typeid(AnalysisT)));
        ASSERT_WITH(it != analysis.end(), std::string("Type ") +
                                              typeid(AnalysisT).name() +
                                              " not found in container.");
        return it->second.cast<AnalysisT>();
    }

    template <typename AnalysisT>
    void removeAnalysis() {
        auto it = analysis.find(std::type_index(typeid(AnalysisT)));
        ASSERT_WITH(it != analysis.end(), std::string("Type ") +
                                              typeid(AnalysisT).name() +
                                              " not found in container.");
        analysis.erase(it);
    }

   private:
    std::vector<std::unique_ptr<Pass>>             passes;
    std::unordered_map<std::type_index, UniqueAny> analysis;
};

class Pass {
    friend class PassManager;
    PassManager* manager;

   public:
    Pass()          = default;
    virtual ~Pass() = default;

    Pass(const Pass&)            = delete;
    Pass& operator=(const Pass&) = delete;

    virtual void doInitialization(Module&) {};
    virtual void doFinalization(Module&) {};
    virtual void runOnModule(Module&) = 0;

   protected:
    template <typename AnalysisT, typename... ArgsT>
    void setAnalysis(ArgsT&&... args) {
        return manager->setAnalysis<AnalysisT>(std::forward<ArgsT>(args)...);
    }

    template <typename AnalysisT>
    AnalysisT& getAnalysis() {
        return manager->getAnalysis<AnalysisT>();
    }

    template <typename AnalysisT>
    void removeAnalysis() {
        manager->removeAnalysis<AnalysisT>();
    }
};

class ImmutablePass : public Pass {
   private:
    void runOnModule(Module&) override {}
};

class FunctionPass : public Pass {
   public:
    virtual void runOnFunction(Function&) = 0;

   private:
    void runOnModule(Module&) override;
};
}  // namespace ir
