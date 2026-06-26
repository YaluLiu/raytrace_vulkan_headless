#include "usd_animation_source.h"

#include "usd_physics_replay_source.h"

#include <memory>
#include <utility>
#include <vector>

namespace headless_training
{
namespace
{
class PhysicsFrameAnimationAdapter final : public AnimationStateSource
{
public:
  explicit PhysicsFrameAnimationAdapter(std::unique_ptr<PhysicsFrameSource> source)
      : m_source(std::move(source))
  {
  }

  bool nextFrame(AnimationFrameUpdates& updates) override
  {
    PhysicsFrameSnapshot snapshot;
    const PhysicsFrameStatus status = m_source->nextFrame(snapshot);
    if(status != PhysicsFrameStatus::Advanced)
    {
      updates = {};
      return false;
    }
    updates = std::move(snapshot.updates);
    return true;
  }

  void reset() override { m_source->reset(); }

private:
  std::unique_ptr<PhysicsFrameSource> m_source;
};
} // namespace

std::unique_ptr<AnimationStateSource> LoadUsdAnimationSource(const std::filesystem::path& usdPath,
                                                            const TrainingSceneDescription& scene)
{
  std::unique_ptr<PhysicsFrameSource> source = LoadUsdPhysicsReplaySource(usdPath, scene);
  if(!source)
  {
    return nullptr;
  }
  return std::make_unique<PhysicsFrameAnimationAdapter>(std::move(source));
}

} // namespace headless_training
