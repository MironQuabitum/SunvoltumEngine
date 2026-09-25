#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace SunvoltumPhysics {

    // Снимок физического состояния тела в определенный момент времени
    struct TransformSnapshot {
        double timestamp{ 0.0 }; // Время в секундах (сетевое или клиентское монотонное)
        Transform transform;
        Vector3 linearVelocity{ 0.0f, 0.0f, 0.0f };
        Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    };

    // Настройки интерполятора
    struct InterpolatorConfig {
        double interpolationDelay{ 0.080 }; // 80 мс задержки интерполяции (~2.4 сетевых тика при 30Hz)
        double maxExtrapolationTime{ 0.150 }; // Максимальное время прогноза при пропаже пакетов (150 мс)
        float teleportDistanceSq{ 16.0f * 16.0f }; // Порог телепортации
        Vector3 gravity{ 0.0f, -196.2f, 0.0f }; // Гравитация для экстраполяции (studs/s^2)
        bool applyGravityInExtrapolation{ false }; // По умолчанию выключено для предотвращения проваливания
    };

    class NetworkInterpolator {
    public:
        static constexpr size_t BUFFER_SIZE = 32;

        explicit NetworkInterpolator(const InterpolatorConfig& config = InterpolatorConfig())
            : m_config(config) {}

        void SetConfig(const InterpolatorConfig& config) { m_config = config; }
        const InterpolatorConfig& GetConfig() const { return m_config; }

        void SetGravity(const Vector3& gravity) { m_config.gravity = gravity; }

        // Сбросить интерполятор к заданному состоянию (при спавне, телепорте или реконнекте)
        void Reset(const Transform& transform, const Vector3& linVel = Vector3::Zero, const Vector3& angVel = Vector3::Zero, double timestamp = 0.0) {
            m_count = 0;
            m_head = 0;
            m_initialized = true;

            TransformSnapshot snap;
            snap.timestamp = timestamp;
            snap.transform = transform;
            snap.linearVelocity = linVel;
            snap.angularVelocity = angVel;

            PushSnapshotInternal(snap);
            m_lastEvaluatedTransform = transform;
            m_lastEvaluatedLinearVel = linVel;
            m_lastEvaluatedAngularVel = angVel;
        }

        // Добавить новый снимок состояния из сети
        void PushSnapshot(const TransformSnapshot& snapshot) {
            if (!m_initialized) {
                Reset(snapshot.transform, snapshot.linearVelocity, snapshot.angularVelocity, snapshot.timestamp);
                return;
            }

            // Проверка на телепортацию
            const TransformSnapshot* newest = GetNewestSnapshot();
            if (newest) {
                float distSq = (snapshot.transform.position - newest->transform.position).LengthSquared();
                if (distSq > m_config.teleportDistanceSq) {
                    Reset(snapshot.transform, snapshot.linearVelocity, snapshot.angularVelocity, snapshot.timestamp);
                    return;
                }

                // Отсеиваем пакеты, пришедшие из прошлого (out-of-order)
                if (snapshot.timestamp <= newest->timestamp) {
                    return;
                }
            }

            PushSnapshotInternal(snapshot);
        }

        // Вычислить интерполированное / экстраполированное состояние на момент времени targetTime
        bool Evaluate(double targetTime, Transform& outTransform, Vector3& outLinearVel, Vector3& outAngularVel) {
            if (m_count == 0) return false;

            double renderTime = targetTime - m_config.interpolationDelay;

            // Если у нас только 1 снимок (начало потока или после сброса) — удерживаем его без экстраполяции, пока не накопится хотя бы 2 снимка
            if (m_count == 1) {
                const auto& snap = m_buffer[GetIndex(m_head - 1)];
                outTransform = snap.transform;
                outLinearVel = snap.linearVelocity;
                outAngularVel = snap.angularVelocity;
                m_lastEvaluatedTransform = outTransform;
                m_lastEvaluatedLinearVel = outLinearVel;
                m_lastEvaluatedAngularVel = outAngularVel;
                return true;
            }

            const TransformSnapshot* newest = GetNewestSnapshot();
            const TransformSnapshot* oldest = GetOldestSnapshot();

            // Случай 1: renderTime старше самого старого снимка -> берем самый старый
            if (renderTime <= oldest->timestamp) {
                outTransform = oldest->transform;
                outLinearVel = oldest->linearVelocity;
                outAngularVel = oldest->angularVelocity;
                m_lastEvaluatedTransform = outTransform;
                m_lastEvaluatedLinearVel = outLinearVel;
                m_lastEvaluatedAngularVel = outAngularVel;
                return true;
            }

            // Случай 2: renderTime новее самого нового снимка -> Физическая Экстраполяция (Dead Reckoning)
            if (renderTime > newest->timestamp) {
                double dt = renderTime - newest->timestamp;
                if (dt <= m_config.maxExtrapolationTime) {
                    Extrapolate(*newest, static_cast<float>(dt), outTransform, outLinearVel, outAngularVel);
                } else {
                    // Превышен лимит экстраполяции — останавливаем на граничном значении экстраполяции
                    Extrapolate(*newest, static_cast<float>(m_config.maxExtrapolationTime), outTransform, outLinearVel, outAngularVel);
                    outLinearVel = Vector3::Zero;
                    outAngularVel = Vector3::Zero;
                }
                m_lastEvaluatedTransform = outTransform;
                m_lastEvaluatedLinearVel = outLinearVel;
                m_lastEvaluatedAngularVel = outAngularVel;
                return true;
            }

            // Случай 3: renderTime лежит между двумя снимками в буфере -> Кубический Эрмит + Slerp
            for (size_t i = m_count - 1; i > 0; --i) {
                const auto& snapB = m_buffer[GetIndex(m_head - (m_count - i))];
                const auto& snapA = m_buffer[GetIndex(m_head - (m_count - i) - 1)];

                if (renderTime >= snapA.timestamp && renderTime <= snapB.timestamp) {
                    double span = snapB.timestamp - snapA.timestamp;
                    float alpha = (span > 1e-6) ? static_cast<float>((renderTime - snapA.timestamp) / span) : 0.0f;
                    alpha = std::clamp(alpha, 0.0f, 1.0f);

                    InterpolateHermite(snapA, snapB, alpha, static_cast<float>(span), outTransform, outLinearVel, outAngularVel);
                    m_lastEvaluatedTransform = outTransform;
                    m_lastEvaluatedLinearVel = outLinearVel;
                    m_lastEvaluatedAngularVel = outAngularVel;
                    return true;
                }
            }

            outTransform = newest->transform;
            outLinearVel = newest->linearVelocity;
            outAngularVel = newest->angularVelocity;
            m_lastEvaluatedTransform = outTransform;
            m_lastEvaluatedLinearVel = outLinearVel;
            m_lastEvaluatedAngularVel = outAngularVel;
            return true;
        }

        const Transform& GetLastEvaluatedTransform() const { return m_lastEvaluatedTransform; }
        const Vector3& GetLastEvaluatedLinearVelocity() const { return m_lastEvaluatedLinearVel; }
        const Vector3& GetLastEvaluatedAngularVelocity() const { return m_lastEvaluatedAngularVel; }
        bool IsInitialized() const { return m_initialized; }

    private:
        size_t GetIndex(size_t pos) const {
            return pos % BUFFER_SIZE;
        }

        void PushSnapshotInternal(const TransformSnapshot& snap) {
            m_buffer[m_head % BUFFER_SIZE] = snap;
            ++m_head;
            if (m_count < BUFFER_SIZE) {
                ++m_count;
            }
        }

        const TransformSnapshot* GetNewestSnapshot() const {
            if (m_count == 0) return nullptr;
            return &m_buffer[GetIndex(m_head - 1)];
        }

        const TransformSnapshot* GetOldestSnapshot() const {
            if (m_count == 0) return nullptr;
            return &m_buffer[GetIndex(m_head - m_count)];
        }

        // Кубическая интерполяция Эрмита для позиции с учетом касательных скоростей vA и vB
        void InterpolateHermite(
            const TransformSnapshot& a,
            const TransformSnapshot& b,
            float t,
            float dt,
            Transform& outTransform,
            Vector3& outLinVel,
            Vector3& outAngVel) const
        {
            float t2 = t * t;
            float t3 = t2 * t;

            // Базисные функции Эрмита
            float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            float h10 = t3 - 2.0f * t2 + t;
            float h01 = -2.0f * t3 + 3.0f * t2;
            float h11 = t3 - t2;

            Vector3 m0 = a.linearVelocity * dt;
            Vector3 m1 = b.linearVelocity * dt;

            outTransform.position = h00 * a.transform.position +
                                    h10 * m0 +
                                    h01 * b.transform.position +
                                    h11 * m1;

            // Производная по времени для интерполированной линейной скорости
            if (dt > 1e-5f) {
                float dh00 = 6.0f * t2 - 6.0f * t;
                float dh10 = 3.0f * t2 - 4.0f * t + 1.0f;
                float dh01 = -6.0f * t2 + 6.0f * t;
                float dh11 = 3.0f * t2 - 2.0f * t;

                outLinVel = (dh00 * a.transform.position +
                             dh10 * m0 +
                             dh01 * b.transform.position +
                             dh11 * m1) * (1.0f / dt);
            } else {
                outLinVel = a.linearVelocity * (1.0f - t) + b.linearVelocity * t;
            }

            // Slerp для вращения
            outTransform.rotation = Quaternion::Slerp(a.transform.rotation, b.transform.rotation, t);

            // Линейная интерполяция угловой скорости
            outAngVel = a.angularVelocity * (1.0f - t) + b.angularVelocity * t;
        }

        // Физическая экстраполяция (Dead Reckoning) с учетом затухания и проверки покоя
        void Extrapolate(
            const TransformSnapshot& snap,
            float dt,
            Transform& outTransform,
            Vector3& outLinVel,
            Vector3& outAngVel) const
        {
            Vector3 effectiveGravity = Vector3::Zero;

            // Применяем гравитацию только если явно включено и объект реально падает в воздухе (Vy < -0.5f)
            if (m_config.applyGravityInExtrapolation && snap.linearVelocity.y < -0.5f) {
                effectiveGravity = m_config.gravity;
            }

            // x(t) = x0 + v0 * dt + 0.5 * g * dt^2
            outTransform.position = snap.transform.position + snap.linearVelocity * dt + 0.5f * effectiveGravity * (dt * dt);

            // v(t) = v0 + g * dt
            outLinVel = snap.linearVelocity + effectiveGravity * dt;

            // Вращение: q(t) = dq(w * dt) * q0
            float wLen = snap.angularVelocity.Length();
            if (wLen > EPSILON) {
                Quaternion dq = Quaternion::FromAxisAngle(snap.angularVelocity * (1.0f / wLen), wLen * dt);
                outTransform.rotation = (dq * snap.transform.rotation).Normalized();
            } else {
                outTransform.rotation = snap.transform.rotation;
            }

            outAngVel = snap.angularVelocity;
        }

    private:
        InterpolatorConfig m_config;
        std::array<TransformSnapshot, BUFFER_SIZE> m_buffer{};
        size_t m_head{ 0 };
        size_t m_count{ 0 };
        bool m_initialized{ false };

        Transform m_lastEvaluatedTransform;
        Vector3 m_lastEvaluatedLinearVel{ 0.0f, 0.0f, 0.0f };
        Vector3 m_lastEvaluatedAngularVel{ 0.0f, 0.0f, 0.0f };
    };

} // namespace SunvoltumPhysics
