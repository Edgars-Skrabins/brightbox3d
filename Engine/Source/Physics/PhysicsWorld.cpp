#include "Physics/PhysicsWorld.h"

#include "Core/Log.h"
#include "Geometry/ModelAsset.h"
#include "Level/Level.h"
#include "Physics/Colliders.h"
#include "Physics/CollisionInfo.h"
#include "Physics/Rigidbody.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace BrightBox3D::Physics
{
    namespace
    {
        constexpr JPH::ObjectLayer StaticLayer = 0;
        constexpr JPH::ObjectLayer MovingLayer = 1;
        const JPH::BroadPhaseLayer StaticBroadPhase(0);
        const JPH::BroadPhaseLayer MovingBroadPhase(1);

        class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
        {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override { return layer == StaticLayer ? StaticBroadPhase : MovingBroadPhase; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override { return layer == StaticBroadPhase ? "Static" : "Moving"; }
#endif
        };

        class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override
            {
                return layer == MovingLayer || broadPhase == MovingBroadPhase;
            }
        };

        class ObjectPairFilter final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
            {
                return a == MovingLayer || b == MovingLayer;
            }
        };

        struct JoltRuntime
        {
            std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
            std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
            BroadPhaseLayers broadPhaseLayers;
            ObjectVsBroadPhaseFilter objectVsBroadPhase;
            ObjectPairFilter objectPair;
        };

        JoltRuntime& Runtime()
        {
            static JoltRuntime* runtime = []
            {
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
                auto* created = new JoltRuntime();
                created->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
                int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
                created->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);
                return created;
            }();
            return *runtime;
        }

        JPH::Vec3 ToJolt(const Math::Vector3& value)
        {
            return JPH::Vec3(value.x, value.y, value.z);
        }

        JPH::Quat ToJolt(const Math::Quaternion& value)
        {
            return JPH::Quat(value.x, value.y, value.z, value.w).Normalized();
        }

        Math::Vector3 FromJolt(JPH::Vec3Arg value)
        {
            return { value.GetX(), value.GetY(), value.GetZ() };
        }

        Math::Quaternion FromJolt(JPH::QuatArg value)
        {
            return { value.GetX(), value.GetY(), value.GetZ(), value.GetW() };
        }

        struct ContactEvent
        {
            JPH::BodyID first;
            JPH::BodyID second;
            Math::Vector3 point;
            Math::Vector3 normal;
            Math::Vector3 relativeVelocity;
            bool added = true;
        };

        class ContactRecorder final : public JPH::ContactListener
        {
        public:
            void OnContactAdded(const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override
            {
                ContactEvent event;
                event.first = first.GetID();
                event.second = second.GetID();
                JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
                event.point = { static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ()) };
                event.normal = FromJolt(manifold.mWorldSpaceNormal);
                event.relativeVelocity = FromJolt(second.GetLinearVelocity() - first.GetLinearVelocity());
                std::lock_guard<std::mutex> lock(m_mutex);
                m_events.push_back(event);
            }

            void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
            {
                ContactEvent event;
                event.first = pair.GetBody1ID();
                event.second = pair.GetBody2ID();
                event.added = false;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_events.push_back(event);
            }

            std::vector<ContactEvent> Take()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                std::vector<ContactEvent> events;
                events.swap(m_events);
                return events;
            }

        private:
            std::mutex m_mutex;
            std::vector<ContactEvent> m_events;
        };

        class IgnoreSensors final : public JPH::BodyFilter
        {
        public:
            bool ShouldCollideLocked(const JPH::Body& body) const override { return !body.IsSensor(); }
        };
    }

    struct PhysicsWorld::Implementation
    {
        std::unique_ptr<JPH::PhysicsSystem> system;
        ContactRecorder contacts;
        struct BodyRecord
        {
            JPH::BodyID solid;
            JPH::BodyID sensor;
            uint64_t transformVersion = 0;
            Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
            MotionType motion = MotionType::Static;
        };
        std::unordered_map<Core::Guid, BodyRecord> records;
        std::unordered_map<uint32_t, Core::Guid> owners;
        std::unordered_set<Core::Guid> dirty;
        std::set<std::pair<uint32_t, uint32_t>> activePairs;
        Math::Vector3 gravity{ 0.0f, -9.81f, 0.0f };
    };

    PhysicsWorld::PhysicsWorld(Level::Level& level) : LevelSubsystem(level), m_implementation(std::make_unique<Implementation>())
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        OnGameEnd();
    }

    PhysicsWorld* PhysicsWorld::Get(const Level::Level* level)
    {
        return level ? level->GetSubsystem<PhysicsWorld>() : nullptr;
    }

    bool PhysicsWorld::IsRunning() const
    {
        return m_implementation->system != nullptr;
    }

    void PhysicsWorld::OnGameStart()
    {
        JoltRuntime& runtime = Runtime();
        auto system = std::make_unique<JPH::PhysicsSystem>();
        system->Init(65536, 0, 65536, 16384, runtime.broadPhaseLayers, runtime.objectVsBroadPhase, runtime.objectPair);
        system->SetGravity(ToJolt(m_implementation->gravity));
        system->SetContactListener(&m_implementation->contacts);
        m_implementation->system = std::move(system);
        GetLevel()->ForEachObject([&](Level::LevelObject& object)
        {
            if (object.GetComponent<Collider>() || object.GetComponent<Rigidbody>())
            {
                m_implementation->dirty.insert(object.GetId());
            }
        });
        OnPreFixedUpdate(0.0f);
        m_implementation->system->OptimizeBroadPhase();
    }

    void PhysicsWorld::OnGameEnd()
    {
        if (!m_implementation->system)
        {
            return;
        }
        JPH::BodyInterface& bodies = m_implementation->system->GetBodyInterface();
        for (auto& [id, record] : m_implementation->records)
        {
            for (JPH::BodyID body : { record.solid, record.sensor })
            {
                if (!body.IsInvalid())
                {
                    bodies.RemoveBody(body);
                    bodies.DestroyBody(body);
                }
            }
        }
        m_implementation->records.clear();
        m_implementation->owners.clear();
        m_implementation->dirty.clear();
        m_implementation->activePairs.clear();
        m_implementation->system.reset();
    }

    void PhysicsWorld::MarkDirty(Level::LevelObject& object)
    {
        m_implementation->dirty.insert(object.GetId());
    }

    void PhysicsWorld::OnComponentAdded(Level::LevelObjectComponent& component)
    {
        if (component.IsA<Collider>() || component.IsA<Rigidbody>())
        {
            MarkDirty(*component.GetOwner());
        }
    }

    void PhysicsWorld::OnComponentRemoved(Level::LevelObjectComponent& component)
    {
        if (!component.IsA<Collider>() && !component.IsA<Rigidbody>())
        {
            return;
        }
        Core::Guid id = component.GetOwner()->GetId();
        m_implementation->dirty.insert(id);
        if (auto* rigidbody = Reflection::Cast<Rigidbody>(&component))
        {
            rigidbody->SetBodyId(0xFFFFFFFFu);
        }
        auto it = m_implementation->records.find(id);
        if (it != m_implementation->records.end() && m_implementation->system)
        {
            JPH::BodyInterface& bodies = m_implementation->system->GetBodyInterface();
            for (JPH::BodyID body : { it->second.solid, it->second.sensor })
            {
                if (!body.IsInvalid())
                {
                    m_implementation->owners.erase(body.GetIndexAndSequenceNumber());
                    bodies.RemoveBody(body);
                    bodies.DestroyBody(body);
                }
            }
            m_implementation->records.erase(it);
        }
    }

    namespace
    {
        JPH::RefConst<JPH::Shape> CreateColliderShape(const Collider& collider, bool dynamic)
        {
            JPH::RefConst<JPH::Shape> shape;
            if (auto* box = Reflection::Cast<BoxCollider>(&collider))
            {
                JPH::Vec3 half = ToJolt(box->GetSize() * 0.5f).Abs();
                half = JPH::Vec3::sMax(half, JPH::Vec3::sReplicate(0.001f));
                float radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin() * 0.5f);
                shape = new JPH::BoxShape(half, radius);
            }
            else if (auto* sphere = Reflection::Cast<SphereCollider>(&collider))
            {
                shape = new JPH::SphereShape(std::max(sphere->GetRadius(), 0.001f));
            }
            else if (auto* capsule = Reflection::Cast<CapsuleCollider>(&collider))
            {
                float radius = std::max(capsule->GetRadius(), 0.001f);
                float halfHeight = std::max(capsule->GetHeight() * 0.5f - radius, 0.001f);
                shape = new JPH::CapsuleShape(halfHeight, radius);
            }
            else if (auto* mesh = Reflection::Cast<MeshCollider>(&collider))
            {
                auto model = mesh->GetModel().Load();
                const Geometry::MeshData* data = model ? model->GetMesh(mesh->GetMeshIndex()) : nullptr;
                if (!data || data->vertices.empty())
                {
                    return nullptr;
                }
                bool convex = mesh->IsConvex() || dynamic;
                if (!mesh->IsConvex() && dynamic)
                {
                    Core::Log::Warning("Dynamic bodies need convex MeshColliders; using a convex hull for '" + collider.GetOwner()->GetName() + "'");
                }
                if (convex)
                {
                    JPH::Array<JPH::Vec3> points;
                    points.reserve(data->vertices.size());
                    for (const Geometry::MeshVertex& vertex : data->vertices)
                    {
                        points.push_back(ToJolt(vertex.position));
                    }
                    JPH::ConvexHullShapeSettings settings(points);
                    JPH::ShapeSettings::ShapeResult result = settings.Create();
                    if (result.HasError())
                    {
                        Core::Log::Error("Convex hull creation failed: " + std::string(result.GetError().c_str()));
                        return nullptr;
                    }
                    shape = result.Get();
                }
                else
                {
                    JPH::VertexList vertices;
                    vertices.reserve(data->vertices.size());
                    for (const Geometry::MeshVertex& vertex : data->vertices)
                    {
                        vertices.push_back(JPH::Float3(vertex.position.x, vertex.position.y, vertex.position.z));
                    }
                    JPH::IndexedTriangleList triangles;
                    triangles.reserve(data->indices.size() / 3);
                    for (size_t i = 0; i + 2 < data->indices.size(); i += 3)
                    {
                        triangles.push_back(JPH::IndexedTriangle(data->indices[i], data->indices[i + 1], data->indices[i + 2]));
                    }
                    JPH::MeshShapeSettings settings(vertices, triangles);
                    JPH::ShapeSettings::ShapeResult result = settings.Create();
                    if (result.HasError())
                    {
                        Core::Log::Error("Mesh collider creation failed: " + std::string(result.GetError().c_str()));
                        return nullptr;
                    }
                    shape = result.Get();
                }
            }
            if (shape && collider.GetCenter().LengthSquared() > 0.0f)
            {
                shape = new JPH::RotatedTranslatedShape(ToJolt(collider.GetCenter()), JPH::Quat::sIdentity(), shape);
            }
            return shape;
        }

        JPH::RefConst<JPH::Shape> CombineShapes(const std::vector<JPH::RefConst<JPH::Shape>>& shapes)
        {
            if (shapes.empty())
            {
                return nullptr;
            }
            if (shapes.size() == 1)
            {
                return shapes.front();
            }
            JPH::StaticCompoundShapeSettings compound;
            for (const auto& shape : shapes)
            {
                compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shape);
            }
            JPH::ShapeSettings::ShapeResult result = compound.Create();
            return result.HasError() ? nullptr : result.Get();
        }

        JPH::RefConst<JPH::Shape> ApplyScale(const JPH::RefConst<JPH::Shape>& shape, const Math::Vector3& scale)
        {
            if (!shape)
            {
                return nullptr;
            }
            JPH::Vec3 scaleVector = ToJolt(scale);
            if (scaleVector.IsClose(JPH::Vec3::sReplicate(1.0f), 1.0e-6f))
            {
                return shape;
            }
            JPH::Vec3 valid = shape->MakeScaleValid(scaleVector);
            return new JPH::ScaledShape(shape, valid);
        }
    }

    void PhysicsWorld::OnPreFixedUpdate(float fixedDeltaTime)
    {
        Implementation& implementation = *m_implementation;
        if (!implementation.system)
        {
            return;
        }
        JPH::BodyInterface& bodies = implementation.system->GetBodyInterface();

        for (auto& [id, record] : implementation.records)
        {
            Level::LevelObject* object = GetLevel()->FindObject(id);
            if (!object)
            {
                continue;
            }
            Math::Vector3 scale = object->GetTransform().GetLossyScale();
            if ((scale - record.scale).LengthSquared() > 1.0e-8f)
            {
                implementation.dirty.insert(id);
            }
        }

        std::unordered_set<Core::Guid> dirty;
        dirty.swap(implementation.dirty);
        for (const Core::Guid& id : dirty)
        {
            auto existing = implementation.records.find(id);
            if (existing != implementation.records.end())
            {
                for (JPH::BodyID body : { existing->second.solid, existing->second.sensor })
                {
                    if (!body.IsInvalid())
                    {
                        implementation.owners.erase(body.GetIndexAndSequenceNumber());
                        bodies.RemoveBody(body);
                        bodies.DestroyBody(body);
                    }
                }
                implementation.records.erase(existing);
            }
            Level::LevelObject* object = GetLevel()->FindObject(id);
            if (!object || object->IsPendingDestroy() || !object->IsActiveInHierarchy())
            {
                continue;
            }
            Rigidbody* rigidbody = object->GetComponent<Rigidbody>();
            if (rigidbody && !rigidbody->IsEnabled())
            {
                rigidbody = nullptr;
            }
            MotionType motion = rigidbody ? rigidbody->GetMotionType() : MotionType::Static;
            std::vector<JPH::RefConst<JPH::Shape>> solids;
            std::vector<JPH::RefConst<JPH::Shape>> sensors;
            float friction = 0.5f;
            float restitution = 0.0f;
            for (Collider* collider : object->GetComponents<Collider>())
            {
                if (!collider->IsEnabled())
                {
                    continue;
                }
                JPH::RefConst<JPH::Shape> shape = CreateColliderShape(*collider, motion == MotionType::Dynamic && !collider->IsTrigger());
                if (!shape)
                {
                    continue;
                }
                (collider->IsTrigger() ? sensors : solids).push_back(shape);
                if (!collider->IsTrigger())
                {
                    friction = collider->GetFriction();
                    restitution = collider->GetRestitution();
                }
            }
            if (solids.empty() && sensors.empty())
            {
                if (rigidbody && motion == MotionType::Dynamic)
                {
                    solids.push_back(new JPH::SphereShape(0.05f));
                }
                else
                {
                    continue;
                }
            }

            Implementation::BodyRecord record;
            record.motion = motion;
            record.scale = object->GetTransform().GetLossyScale();
            const Level::Transform& transform = object->GetTransform();
            JPH::RVec3 position(transform.GetPosition().x, transform.GetPosition().y, transform.GetPosition().z);
            JPH::Quat rotation = ToJolt(transform.GetRotation());

            if (!solids.empty())
            {
                JPH::RefConst<JPH::Shape> shape = ApplyScale(CombineShapes(solids), record.scale);
                JPH::EMotionType motionType = motion == MotionType::Dynamic ? JPH::EMotionType::Dynamic : (motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static);
                JPH::BodyCreationSettings settings(shape, position, rotation, motionType, motion == MotionType::Static ? StaticLayer : MovingLayer);
                settings.mFriction = friction;
                settings.mRestitution = restitution;
                if (rigidbody)
                {
                    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                    settings.mMassPropertiesOverride.mMass = std::max(rigidbody->GetMass(), 0.001f);
                    settings.mGravityFactor = rigidbody->GetGravityScale();
                    settings.mLinearDamping = rigidbody->GetLinearDamping();
                    settings.mAngularDamping = rigidbody->GetAngularDamping();
                    settings.mMotionQuality = rigidbody->GetContinuousCollision() ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
                    if (rigidbody->GetFreezeRotation())
                    {
                        settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
                    }
                }
                record.solid = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
                if (!record.solid.IsInvalid())
                {
                    implementation.owners[record.solid.GetIndexAndSequenceNumber()] = id;
                }
                if (rigidbody)
                {
                    rigidbody->SetBodyId(record.solid.GetIndexAndSequenceNumber());
                }
            }
            if (!sensors.empty())
            {
                JPH::RefConst<JPH::Shape> shape = ApplyScale(CombineShapes(sensors), record.scale);
                JPH::BodyCreationSettings settings(shape, position, rotation, JPH::EMotionType::Kinematic, MovingLayer);
                settings.mIsSensor = true;
                settings.mCollideKinematicVsNonDynamic = true;
                record.sensor = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
                if (!record.sensor.IsInvalid())
                {
                    implementation.owners[record.sensor.GetIndexAndSequenceNumber()] = id;
                }
            }
            record.transformVersion = transform.GetVersion();
            implementation.records[id] = record;
        }

        for (auto& [id, record] : implementation.records)
        {
            Level::LevelObject* object = GetLevel()->FindObject(id);
            if (!object)
            {
                continue;
            }
            const Level::Transform& transform = object->GetTransform();
            bool moved = transform.GetVersion() != record.transformVersion;
            JPH::RVec3 position(transform.GetPosition().x, transform.GetPosition().y, transform.GetPosition().z);
            JPH::Quat rotation = ToJolt(transform.GetRotation());
            if (!record.solid.IsInvalid() && moved)
            {
                if (record.motion == MotionType::Kinematic && fixedDeltaTime > 0.0f)
                {
                    bodies.MoveKinematic(record.solid, position, rotation, fixedDeltaTime);
                }
                else
                {
                    bodies.SetPositionAndRotation(record.solid, position, rotation, JPH::EActivation::Activate);
                }
            }
            if (!record.sensor.IsInvalid() && (moved || record.motion == MotionType::Dynamic))
            {
                if (fixedDeltaTime > 0.0f)
                {
                    bodies.MoveKinematic(record.sensor, position, rotation, fixedDeltaTime);
                }
                else
                {
                    bodies.SetPositionAndRotation(record.sensor, position, rotation, JPH::EActivation::Activate);
                }
            }
            record.transformVersion = transform.GetVersion();
        }
    }

    void PhysicsWorld::OnGameUpdateFixed(float fixedDeltaTime)
    {
        Implementation& implementation = *m_implementation;
        if (!implementation.system || fixedDeltaTime <= 0.0f)
        {
            return;
        }
        JoltRuntime& runtime = Runtime();
        implementation.system->Update(fixedDeltaTime, 1, runtime.tempAllocator.get(), runtime.jobSystem.get());
        JPH::BodyInterface& bodies = implementation.system->GetBodyInterface();
        for (auto& [id, record] : implementation.records)
        {
            if (record.motion != MotionType::Dynamic || record.solid.IsInvalid() || !bodies.IsActive(record.solid))
            {
                continue;
            }
            Level::LevelObject* object = GetLevel()->FindObject(id);
            if (!object)
            {
                continue;
            }
            JPH::RVec3 position;
            JPH::Quat rotation;
            bodies.GetPositionAndRotation(record.solid, position, rotation);
            object->GetTransform().SetPosition({ static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()) });
            object->GetTransform().SetRotation(FromJolt(rotation));
            record.transformVersion = object->GetTransform().GetVersion();
        }

        for (const ContactEvent& event : implementation.contacts.Take())
        {
            auto firstOwner = implementation.owners.find(event.first.GetIndexAndSequenceNumber());
            auto secondOwner = implementation.owners.find(event.second.GetIndexAndSequenceNumber());
            if (firstOwner == implementation.owners.end() || secondOwner == implementation.owners.end() || firstOwner->second == secondOwner->second)
            {
                continue;
            }
            std::pair<uint32_t, uint32_t> key(std::min(event.first.GetIndexAndSequenceNumber(), event.second.GetIndexAndSequenceNumber()), std::max(event.first.GetIndexAndSequenceNumber(), event.second.GetIndexAndSequenceNumber()));
            if (event.added)
            {
                if (!implementation.activePairs.insert(key).second)
                {
                    continue;
                }
            }
            else if (implementation.activePairs.erase(key) == 0)
            {
                continue;
            }
            Level::LevelObject* first = GetLevel()->FindObject(firstOwner->second);
            Level::LevelObject* second = GetLevel()->FindObject(secondOwner->second);
            if (!first || !second)
            {
                continue;
            }
            auto isSensor = [&](JPH::BodyID body)
            {
                auto record = implementation.records.find(implementation.owners[body.GetIndexAndSequenceNumber()]);
                return record != implementation.records.end() && record->second.sensor == body;
            };
            bool trigger = isSensor(event.first) || isSensor(event.second);
            auto dispatch = [&](Level::LevelObject& target, Level::LevelObject& other, const Math::Vector3& normal)
            {
                CollisionInfo info;
                info.other = &other;
                info.point = event.point;
                info.normal = normal;
                info.relativeVelocity = event.relativeVelocity;
                info.isTrigger = trigger;
                if (trigger)
                {
                    event.added ? target.OnTriggerEnter(info) : target.OnTriggerExit(info);
                }
                else
                {
                    event.added ? target.OnCollisionEnter(info) : target.OnCollisionExit(info);
                }
                for (const auto& component : target.GetComponents())
                {
                    if (!component->IsEnabled() || component->IsPendingDestroy())
                    {
                        continue;
                    }
                    if (trigger)
                    {
                        event.added ? component->OnTriggerEnter(info) : component->OnTriggerExit(info);
                    }
                    else
                    {
                        event.added ? component->OnCollisionEnter(info) : component->OnCollisionExit(info);
                    }
                }
            };
            dispatch(*first, *second, -event.normal);
            dispatch(*second, *first, event.normal);
        }
    }

    bool PhysicsWorld::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit, bool includeTriggers) const
    {
        Implementation& implementation = *m_implementation;
        if (!implementation.system)
        {
            return false;
        }
        Math::Vector3 normalized = direction.Normalized();
        JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(normalized * maxDistance));
        JPH::RayCastResult result;
        IgnoreSensors ignoreSensors;
        JPH::BodyFilter allBodies;
        bool found = implementation.system->GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, includeTriggers ? allBodies : static_cast<const JPH::BodyFilter&>(ignoreSensors));
        if (!found)
        {
            return false;
        }
        auto owner = implementation.owners.find(result.mBodyID.GetIndexAndSequenceNumber());
        hit.object = owner == implementation.owners.end() ? nullptr : GetLevel()->FindObject(owner->second);
        hit.distance = result.mFraction * maxDistance;
        hit.point = origin + normalized * hit.distance;
        JPH::BodyLockRead lock(implementation.system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded())
        {
            hit.normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
        }
        return true;
    }

    bool PhysicsWorld::Raycast(const Level::Level* level, const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit)
    {
        PhysicsWorld* world = Get(level);
        return world && world->Raycast(origin, direction, maxDistance, hit);
    }

    void PhysicsWorld::SetGravity(const Math::Vector3& gravity)
    {
        m_implementation->gravity = gravity;
        if (m_implementation->system)
        {
            m_implementation->system->SetGravity(ToJolt(gravity));
        }
    }

    Math::Vector3 PhysicsWorld::GetGravity() const
    {
        return m_implementation->gravity;
    }

    namespace
    {
        JPH::BodyID BodyOf(const Rigidbody& body)
        {
            return JPH::BodyID(body.GetBodyId());
        }
    }

    Math::Vector3 PhysicsWorld::GetLinearVelocity(const Rigidbody& body) const
    {
        if (!m_implementation->system || BodyOf(body).IsInvalid())
        {
            return {};
        }
        return FromJolt(m_implementation->system->GetBodyInterface().GetLinearVelocity(BodyOf(body)));
    }

    void PhysicsWorld::SetLinearVelocity(Rigidbody& body, const Math::Vector3& velocity)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().SetLinearVelocity(BodyOf(body), ToJolt(velocity));
        }
    }

    Math::Vector3 PhysicsWorld::GetAngularVelocity(const Rigidbody& body) const
    {
        if (!m_implementation->system || BodyOf(body).IsInvalid())
        {
            return {};
        }
        return FromJolt(m_implementation->system->GetBodyInterface().GetAngularVelocity(BodyOf(body)));
    }

    void PhysicsWorld::SetAngularVelocity(Rigidbody& body, const Math::Vector3& velocity)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().SetAngularVelocity(BodyOf(body), ToJolt(velocity));
        }
    }

    void PhysicsWorld::AddForce(Rigidbody& body, const Math::Vector3& force)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().AddForce(BodyOf(body), ToJolt(force));
        }
    }

    void PhysicsWorld::AddImpulse(Rigidbody& body, const Math::Vector3& impulse)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().AddImpulse(BodyOf(body), ToJolt(impulse));
        }
    }

    void PhysicsWorld::AddTorque(Rigidbody& body, const Math::Vector3& torque)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().AddTorque(BodyOf(body), ToJolt(torque));
        }
    }

    void PhysicsWorld::MoveKinematic(Rigidbody& body, const Math::Vector3& position, float deltaTime)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            JPH::BodyInterface& bodies = m_implementation->system->GetBodyInterface();
            bodies.MoveKinematic(BodyOf(body), JPH::RVec3(position.x, position.y, position.z), bodies.GetRotation(BodyOf(body)), std::max(deltaTime, 1.0e-4f));
        }
    }

    bool PhysicsWorld::IsSleeping(const Rigidbody& body) const
    {
        if (!m_implementation->system || BodyOf(body).IsInvalid())
        {
            return false;
        }
        return !m_implementation->system->GetBodyInterface().IsActive(BodyOf(body));
    }

    void PhysicsWorld::WakeUp(Rigidbody& body)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().ActivateBody(BodyOf(body));
        }
    }

    void PhysicsWorld::SetGravityScale(Rigidbody& body, float scale)
    {
        if (m_implementation->system && !BodyOf(body).IsInvalid())
        {
            m_implementation->system->GetBodyInterface().SetGravityFactor(BodyOf(body), scale);
        }
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                Level::LevelSubsystemRegistry::Register("Physics", [](Level::Level& level) { return std::make_unique<PhysicsWorld>(level); });
            }
        };

        Registration s_registration;
    }
}
