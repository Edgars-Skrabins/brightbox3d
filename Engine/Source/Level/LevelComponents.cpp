#include "Level/Level.h"

namespace BrightBox3D::Level
{
    void Level::LoadComponent(LevelObjectComponent& component)
    {
        if (component.m_engineState.hasLoaded || component.m_engineState.isPendingDestroy)
        {
            return;
        }
        component.m_engineState.hasLoaded = true;
        component.OnGameLoad();
    }

    void Level::ApplyEnabledState(LevelObject& object, EnabledNotification notification)
    {
        bool objectEnabled = object.IsActiveInHierarchy() && !object.m_engineState.isPendingDestroy;
        if (objectEnabled && !object.m_engineState.hasLoaded && m_isPlaying)
        {
            object.m_engineState.hasLoaded = true;
            object.OnGameLoad();
        }
        if (objectEnabled != object.m_engineState.isEnabledState)
        {
            object.m_engineState.isEnabledState = objectEnabled;
            if (notification == EnabledNotification::Notify && m_isPlaying)
            {
                if (objectEnabled)
                {
                    object.OnEnable();
                }
                else
                {
                    object.OnDisable();
                }
            }
        }
        for (size_t i = 0; i < object.m_components.size(); ++i)
        {
            LevelObjectComponent& component = *object.m_components[i];
            bool componentEnabled = objectEnabled && component.m_state.isEnabled && !component.m_engineState.isPendingDestroy;
            if (componentEnabled && m_isPlaying)
            {
                LoadComponent(component);
            }
            if (componentEnabled != component.m_engineState.isEnabledState)
            {
                component.m_engineState.isEnabledState = componentEnabled;
                if (notification == EnabledNotification::Notify && m_isPlaying)
                {
                    if (componentEnabled)
                    {
                        component.OnEnable();
                    }
                    else
                    {
                        component.OnDisable();
                    }
                }
            }
        }
    }

    void Level::RefreshEnabledState(LevelObject& object)
    {
        ApplyEnabledState(object, EnabledNotification::Notify);
        std::vector<LevelObject*> children = object.m_children;
        for (LevelObject* child : children)
        {
            RefreshEnabledState(*child);
        }
    }

    void Level::OnComponentAttached(LevelObjectComponent& component)
    {
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentAdded(component);
        }
        component.OnInitialize();
        if (m_isPlaying && component.m_owner)
        {
            ApplyEnabledState(*component.m_owner, EnabledNotification::Notify);
        }
    }

    void Level::RemoveComponent(LevelObjectComponent& component)
    {
        if (component.m_engineState.isPendingDestroy)
        {
            return;
        }
        if (m_isPlaying && !m_isEnding)
        {
            component.m_engineState.isPendingDestroy = true;
            m_pendingComponentDestroy.push_back(&component);
            return;
        }
        LevelObject* owner = component.m_owner;
        if (m_isPlaying)
        {
            if (component.m_engineState.isEnabledState)
            {
                component.m_engineState.isEnabledState = false;
                component.OnDisable();
            }
            component.OnDestroy();
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentRemoved(component);
        }
        owner->DetachComponent(&component);
    }

    void Level::AttachComponent(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, std::optional<size_t> position)
    {
        object.AttachComponent(std::move(component), position);
    }
}
