#include "Level/Level.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    namespace
    {
        constexpr int MaxFixedStepsPerFrame = 8;
        constexpr float MaxTickDeltaTime = 0.25f;
    }

    void Level::BeginPlay()
    {
        if (m_isPlaying)
        {
            return;
        }
        m_isPlaying = true;
        m_time = 0.0f;
        m_unscaledTime = 0.0f;
        m_fixedAccumulator = 0.0f;
        m_frameCount = 0;
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnGameStart();
        }
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            ActivateForPlay(*m_objects[i]);
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            LoadEngineObject(*m_engineObjects[i]);
        }
    }

    void Level::LoadEngineObject(Reflection::EngineObject& object)
    {
        Reflection::EngineObjectState& state = object.m_engineState;
        if (state.hasLoaded || state.isPendingDestroy)
        {
            return;
        }
        state.hasLoaded = true;
        state.isEnabledState = true;
        object.OnGameLoad();
    }

    void Level::RunPendingStarts()
    {
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (object.m_engineState.isPendingDestroy || !object.m_engineState.isEnabledState)
            {
                continue;
            }
            if (!object.m_engineState.hasStarted)
            {
                object.m_engineState.hasStarted = true;
                object.OnGameStart();
            }
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                LevelObjectComponent& component = *object.m_components[c];
                if (component.m_engineState.isEnabledState && !component.m_engineState.hasStarted && !component.m_engineState.isPendingDestroy)
                {
                    component.m_engineState.hasStarted = true;
                    component.OnGameStart();
                }
            }
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            Reflection::EngineObject* object = m_engineObjects[i].get();
            if (object && object->m_engineState.isEnabledState && !object->m_engineState.hasStarted && !object->m_engineState.isPendingDestroy)
            {
                object->m_engineState.hasStarted = true;
                object->OnGameStart();
            }
        }
    }

    void Level::Update(float deltaTime)
    {
        if (!m_isPlaying)
        {
            return;
        }
        if (m_isPaused)
        {
            m_unscaledDeltaTime = deltaTime;
            m_deltaTime = 0.0f;
            FlushDestroyed();
            return;
        }
        Tick(deltaTime, false);
    }

    void Level::Step(float deltaTime)
    {
        if (m_isPlaying)
        {
            Tick(deltaTime, true);
        }
    }

    void Level::Tick(float deltaTime, bool stepping)
    {
        deltaTime = std::clamp(deltaTime, 0.0f, MaxTickDeltaTime);
        m_unscaledDeltaTime = deltaTime;
        m_deltaTime = deltaTime * m_timeScale;
        m_unscaledTime += deltaTime;
        m_time += m_deltaTime;
        ++m_frameCount;

        RunPendingStarts();

        m_fixedAccumulator += m_deltaTime;
        if (stepping && m_fixedAccumulator < m_fixedDeltaTime)
        {
            m_fixedAccumulator = m_fixedDeltaTime;
        }
        int steps = 0;
        while (m_fixedAccumulator >= m_fixedDeltaTime && steps < MaxFixedStepsPerFrame)
        {
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnPreFixedUpdate(m_fixedDeltaTime);
            }
            DispatchUpdate(&Reflection::EngineObject::OnGameUpdateFixed, m_fixedDeltaTime);
            m_fixedAccumulator -= m_fixedDeltaTime;
            ++steps;
        }
        if (steps == MaxFixedStepsPerFrame)
        {
            m_fixedAccumulator = std::min(m_fixedAccumulator, m_fixedDeltaTime);
        }

        DispatchUpdate(&Reflection::EngineObject::OnGameUpdate, m_deltaTime);
        DispatchUpdate(&Reflection::EngineObject::OnGameUpdateLate, m_deltaTime);
        FlushDestroyed();
    }

    void Level::DispatchUpdate(void (Reflection::EngineObject::*callback)(float), float deltaTime)
    {
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (!IsUpdatable(object))
            {
                continue;
            }
            (object.*callback)(deltaTime);
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                LevelObjectComponent& component = *object.m_components[c];
                if (IsUpdatable(component))
                {
                    (component.*callback)(deltaTime);
                }
            }
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            Reflection::EngineObject* object = m_engineObjects[i].get();
            if (object && IsUpdatable(*object))
            {
                (object->*callback)(deltaTime);
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            (subsystem.get()->*callback)(deltaTime);
        }
    }

    bool Level::IsUpdatable(const Reflection::EngineObject& object)
    {
        const Reflection::EngineObjectState& state = object.m_engineState;
        return state.isEnabledState && state.hasStarted && !state.isPendingDestroy;
    }

    void Level::EndPlay()
    {
        if (!m_isPlaying)
        {
            return;
        }
        FlushDestroyed();
        m_isEnding = true;
        for (auto it = m_engineObjects.rbegin(); it != m_engineObjects.rend(); ++it)
        {
            Reflection::EngineObject& object = **it;
            if (object.m_engineState.hasLoaded)
            {
                object.m_engineState.isEnabledState = false;
                object.OnGameEnd();
            }
        }
        for (auto it = m_engineObjects.rbegin(); it != m_engineObjects.rend(); ++it)
        {
            Reflection::EngineObject& object = **it;
            if (object.m_engineState.hasLoaded)
            {
                object.OnDestroy();
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_engineState.isEnabledState)
            {
                object.m_engineState.isEnabledState = false;
                object.OnDisable();
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (!object.m_engineState.hasLoaded)
            {
                continue;
            }
            object.OnGameEnd();
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.hasLoaded)
                {
                    component->OnGameEnd();
                }
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.hasLoaded)
                {
                    component->OnDestroy();
                }
            }
            if (object.m_engineState.hasLoaded)
            {
                object.OnDestroy();
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnGameEnd();
        }
        m_isEnding = false;
        m_isPlaying = false;
    }
}
