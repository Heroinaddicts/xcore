#include "PhysxScene.h"
#include "SafeSystem.h"
#include "PhysxBase.h"

namespace XEngine {
    PhysxScene::PhysxScene(
        PxScene* scene,
        const float static_friction,
        const float dynamic_friction,
        const float restitution)
        : _scene(scene),
        _material(g_pxphysics->createMaterial(static_friction, dynamic_friction, restitution)) {

        _scene->userData = this;
        _scene->setSimulationEventCallback(this);
        _scene->setCCDContactModifyCallback(this);
        _scene->setContactModifyCallback(this);
    }

    void PhysxScene::CreatePlane(const float nx, const float ny, const float nz, const float distance, Api::iPhysxContext* const context) {
        PxRigidStatic* groundPlane = PxCreatePlane(*g_pxphysics, PxPlane(nx, ny, nz, distance), *_material);
        if (nullptr == groundPlane) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        PxShape* shape = nullptr;
        groundPlane->getShapes(&shape, sizeof(shape));
        PxFilterData data = shape->getQueryFilterData();
        _scene->addActor(*groundPlane);
        PhysxBase::Create(this, shape, groundPlane, context);
    }

    void PhysxScene::CreateBox(const eRigType type, const Vector3& pos, const Quaternion& qt, const Vector3& size, Api::iPhysxContext* const context) {
        PxShape* shape = g_pxphysics->createShape(PxBoxGeometry(size.x / 2.0f, size.y / 2.0f, size.z / 2.0f), *_material);
        if (nullptr == shape) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        PxFilterData fd = shape->getSimulationFilterData();
        shape->setSimulationFilterData(fd);

        PxRigidDynamic* body = g_pxphysics->createRigidDynamic(PxTransform(PxVec3(pos.x, pos.y, pos.z)));
        if (nullptr == body) {
            context ? context->OnCreated(false) : void(0);
            shape->release();
            return;
        }

        body->userData = context;
        body->attachShape(*shape);
        body->setActorFlag(PxActorFlag::eVISUALIZATION, true);
        _scene->addActor(*body);
        shape->release();

        context->SetPosition(pos);
        context->SetRotation(qt.EulerAngles());
        PhysxBase::Create(this, shape, body, context);
    }

    void PhysxScene::CreateCapsule(const eRigType type, const Vector3& pos, const Quaternion& qt, const float radius, const float height, Api::iPhysxContext* const context) {
        PxShape* shape = g_pxphysics->createShape(PxCapsuleGeometry(radius, height / 2.0f), *_material);
        if (nullptr == shape) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        PxRigidDynamic* body = g_pxphysics->createRigidDynamic(PxTransform(PxVec3(pos.x, pos.y, pos.z)));
        if (nullptr == body) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        body->attachShape(*shape);
        _scene->addActor(*body);
        shape->release();

        context->SetPosition(pos);
        context->SetRotation(qt.EulerAngles());
        PhysxBase::Create(this, shape, body, context);
    }

    void PhysxScene::CreateConvexMesh(const eRigType type, const Quaternion& qt, Api::iPhysxContext* const context) {
    }

    void PhysxScene::CreateTriangleMesh(
        const eRigType type,
        const Vector3& pos,
        const Quaternion& qt,
        const Vector3& scale,
        const X3DObj* obj,
        Api::iPhysxContext* context
    ) {
        PxRigidActor* actor = nullptr;
        switch (type) {
        case eRigType::Dynamic: {
            actor = g_pxphysics->createRigidDynamic(PxTransform(pos.x, pos.y, pos.z, PxQuat(qt.x, qt.y, qt.z, qt.w)));
            break;
        }
        case eRigType::Static: {
            actor = g_pxphysics->createRigidStatic(PxTransform(pos.x, pos.y, pos.z, PxQuat(qt.x, qt.y, qt.z, qt.w)));
            break;
        }
        }

        if (nullptr == actor) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        const PxU32 numVertices = obj->GetV().size();
        const PxU32 numTriangles = obj->GetF().size();

        PxVec3* vertices = (PxVec3*)_alloca(sizeof(PxVec3) * numVertices);
        PxU32* indices = (PxU32*)_alloca(sizeof(PxU32) * numTriangles * 3);

        // 加载顶点
        for (int i = 0; i < numVertices; ++i) {
            PxVec3 vectmp(obj->GetV()[i].x * scale.x, obj->GetV()[i].y * scale.y, obj->GetV()[i].z * scale.z);
            vertices[i] = vectmp;
        }

        // 加载面
        auto faceIt = obj->GetF().begin();
        for (int i = 0; i < numTriangles && faceIt != obj->GetF().end(); faceIt++, ++i) {
            indices[i * 3 + 0] = (*faceIt)[0].u;
            indices[i * 3 + 1] = (*faceIt)[1].u;
            if ((*faceIt).size() >= 3)
                indices[i * 3 + 2] = (*faceIt)[2].u;
        }

        PxTriangleMeshDesc meshDesc;
        meshDesc.points.count = numVertices;
        meshDesc.points.data = vertices;
        meshDesc.points.stride = sizeof(PxVec3);

        meshDesc.triangles.count = numTriangles;
        meshDesc.triangles.data = indices;
        meshDesc.triangles.stride = sizeof(PxU32) * 3;
        PxTriangleMesh* mesh = g_cooking->createTriangleMesh(meshDesc, g_pxphysics->getPhysicsInsertionCallback());
        if (nullptr == mesh) {
            context ? context->OnCreated(false) : void(0);
            return;
        }

        PxTriangleMeshGeometry geom(mesh);
        PxShape* shape = g_pxphysics->createShape(geom, *_material);
        if (nullptr == shape) {
            mesh->release();
            context ? context->OnCreated(false) : void(0);
            return;
        }

        // 设置厚度， 相当于多了一层 0.03厚的皮肤，也就是为了提前预判
        shape->setContactOffset(0.03f);
        // A negative rest offset helps to avoid jittering when the deformed mesh moves away from objects resting on it.
        // 允许穿透的厚度，当穿透指定的厚度后，就是发生弹开等动作 -0.02f 负数代表穿透后，正数代表穿透前
        shape->setRestOffset(-0.02);

        actor->attachShape(*shape);
        shape->release();
        actor->userData = context;
        _scene->addActor(*actor);
        PhysxBase::Create(this, shape, actor, context);
    }

    bool PhysxScene::Raycast(const Ray& ray, const float distance, int layerMask, const eQueryTriggerInteraction queryTriggerInteraction, RaycastHit& hit) {
        return false;
    }

    void PhysxScene::Simulate(const float elapsed_time) {
        _scene->simulate(elapsed_time);
    }

    bool PhysxScene::FetchResults(bool block) {
        return _scene->fetchResults(block);
    }

    void PhysxScene::Run(void* constext) {

    }

    void PhysxScene::onCCDContactModify(PxContactModifyPair* const pairs, PxU32 count) {
        printf("PhysxScene::onCCDContactModify, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onContactModify(PxContactModifyPair* const pairs, PxU32 count) {
        printf("PhysxScene::onContactModify, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) {
        printf("PhysxScene::onConstraintBreak, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onWake(PxActor** actors, PxU32 count) {
        printf("PhysxScene::onWake, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onSleep(PxActor** actors, PxU32 count) {
        printf("PhysxScene::onSleep, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) {
        printf("PhysxScene::onContact, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onTrigger(PxTriggerPair* pairs, PxU32 count) {
        printf("PhysxScene::onTrigger, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }

    void PhysxScene::onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) {
        printf("PhysxScene::onAdvance, %lld\n", SafeSystem::Process::GetCurrentThreadID());
    }
}
