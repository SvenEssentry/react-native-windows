// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "RefreshControlManager.h"

#include <UI.Xaml.Controls.h>
#include <Utils/Helpers.h>
#include <Views/ShadowNodeBase.h>

namespace winrt {
using namespace xaml;
using namespace xaml::Controls;
using namespace Windows::Foundation;
} // namespace winrt

namespace react::uwp {

class RefreshControlShadowNode : public ShadowNodeBase {
  using Super = ShadowNodeBase;

 public:
  RefreshControlShadowNode(){};
  void createView() override;
  void updateProperties(const folly::dynamic &&props) override;

 private:
  winrt::RefreshContainer::RefreshRequested_revoker m_refreshRequestedRevoker{};
  winrt::Deferral m_refreshDeferral{nullptr};
};

void RefreshControlShadowNode::createView() {
  Super::createView();
  if (auto refreshContainer = GetView().try_as<winrt::RefreshContainer>()) {
    m_refreshRequestedRevoker =
        refreshContainer.RefreshRequested(winrt::auto_revoke, [this](auto &&, winrt::RefreshRequestedEventArgs args) {
          m_refreshDeferral = args.GetDeferral();
          folly::dynamic eventData = folly::dynamic::object();
          GetViewManager()->DispatchEvent(m_tag, "topOnRefresh", std::move(eventData));
        });
  }
}

void RefreshControlShadowNode::updateProperties(const folly::dynamic &&props) {
  if (auto refreshContainer = GetView().try_as<winrt::RefreshContainer>()) {
    for (const auto &pair : props.items()) {
      const std::string &propertyName = pair.first.getString();
      if (propertyName == "flexDirection") {
        const folly::dynamic &propertyValue = pair.second;
        if (propertyValue.isString() && propertyValue.asString() == "column") { // vertical scrollView
          refreshContainer.PullDirection(winrt::RefreshPullDirection::TopToBottom);
        } else {
          refreshContainer.PullDirection(winrt::RefreshPullDirection::LeftToRight);
        }
      } else if (propertyName == "refreshing") {
        const folly::dynamic &propertyValue = pair.second;
        if (propertyValue.isBool()) {
          bool refreshing = propertyValue.asBool();
          if (!refreshing && m_refreshDeferral) {
            m_refreshDeferral.Complete();
            m_refreshDeferral = nullptr;
          }
        }
      }
    }
  }

  Super::updateProperties(std::move(props));
}

RefreshControlViewManager::RefreshControlViewManager(const Mso::React::IReactContext &context) : Super(context) {}

facebook::react::ShadowNode *RefreshControlViewManager::createShadow() const {
  return new RefreshControlShadowNode();
}

const char *RefreshControlViewManager::GetName() const {
  return "RCTRefreshControl";
}

XamlView RefreshControlViewManager::CreateViewCore(int64_t /*tag*/) {
  if (IsRS4OrHigher()) {
    // refreshContainer is supported >= RS4
    return winrt::RefreshContainer();
  } else {
    // just return a grid if refreshContainer is not supported
    return winrt::Grid();
  }
}

void RefreshControlViewManager::AddView(const XamlView &parent, const XamlView &child, int64_t /*index*/) {
  if (auto refreshContainer = parent.try_as<winrt::RefreshContainer>()) {
    refreshContainer.Content(child.as<winrt::ScrollViewer>());
  } else if (auto grid = parent.try_as<winrt::Grid>()) {
    grid.Children().Append(child.as<winrt::ScrollViewer>());
  }
}

folly::dynamic RefreshControlViewManager::GetNativeProps() const {
  auto props = Super::GetNativeProps();

  props.update(folly::dynamic::object("refreshing", "boolean"));

  return props;
}

folly::dynamic RefreshControlViewManager::GetExportedCustomDirectEventTypeConstants() const {
  auto directEvents = Super::GetExportedCustomDirectEventTypeConstants();
  directEvents["topOnRefresh"] = folly::dynamic::object("registrationName", "onRefresh");
  return directEvents;
}
} // namespace react::uwp
