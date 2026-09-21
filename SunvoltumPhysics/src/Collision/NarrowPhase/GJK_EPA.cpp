#include "SunvoltumPhysics/Collision/NarrowPhase/GJK_EPA.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace SunvoltumPhysics {
namespace GJK {

    namespace {

        constexpr int kMaxGjkIters = 32;
        constexpr int kMaxEpaIters = 64;
        constexpr int kMaxEpaVerts = 64;
        constexpr int kMaxEpaFaces = 128;
        constexpr float kEpaTol = 1e-4f;

        struct SupportVertex {
            Vector3 p{ 0.0f, 0.0f, 0.0f };
            Vector3 a{ 0.0f, 0.0f, 0.0f };
            Vector3 b{ 0.0f, 0.0f, 0.0f };
        };

        struct Simplex {
            SupportVertex v[4];
            int count{ 0 };
        };

        inline bool SameDir(const Vector3& a, const Vector3& b) {
            return a.Dot(b) > 0.0f;
        }

        inline SupportVertex SupportDiff(const SupportFn& supportA, const SupportFn& supportB, const Vector3& dir) {
            SupportVertex sv;
            sv.a = supportA(dir);
            sv.b = supportB(-dir);
            sv.p = sv.a - sv.b;
            return sv;
        }

        bool NextSimplex(Simplex& s, Vector3& dir) {
            auto set1 = [&](const SupportVertex& a) {
                s.v[0] = a;
                s.count = 1;
            };
            auto set2 = [&](const SupportVertex& a, const SupportVertex& b) {
                s.v[0] = b;
                s.v[1] = a;
                s.count = 2;
            };
            auto set3 = [&](const SupportVertex& a, const SupportVertex& b, const SupportVertex& c) {
                s.v[0] = c;
                s.v[1] = b;
                s.v[2] = a;
                s.count = 3;
            };

            switch (s.count) {
            case 2: {
                const SupportVertex& a = s.v[1];
                const SupportVertex& b = s.v[0];
                Vector3 ab = b.p - a.p;
                Vector3 ao = -a.p;
                if (SameDir(ab, ao)) {
                    dir = ab.Cross(ao).Cross(ab);
                    if (dir.LengthSquared() < EPSILON * EPSILON) {
                        dir = ab.Cross(Vector3(1.0f, 0.0f, 0.0f));
                        if (dir.LengthSquared() < EPSILON * EPSILON) {
                            dir = ab.Cross(Vector3(0.0f, 1.0f, 0.0f));
                        }
                    }
                } else {
                    set1(a);
                    dir = ao;
                }
                return false;
            }
            case 3: {
                const SupportVertex& a = s.v[2];
                const SupportVertex& b = s.v[1];
                const SupportVertex& c = s.v[0];
                Vector3 ab = b.p - a.p;
                Vector3 ac = c.p - a.p;
                Vector3 ao = -a.p;
                Vector3 abc = ab.Cross(ac);

                if (SameDir(abc.Cross(ac), ao)) {
                    if (SameDir(ac, ao)) {
                        set2(a, c);
                        dir = ac.Cross(ao).Cross(ac);
                    } else {
                        set2(a, b);
                        dir = ab.Cross(ao).Cross(ab);
                    }
                    return false;
                }
                if (SameDir(ab.Cross(abc), ao)) {
                    set2(a, b);
                    dir = ab.Cross(ao).Cross(ab);
                    return false;
                }
                if (SameDir(abc, ao)) {
                    dir = abc;
                } else {
                    set3(a, c, b);
                    dir = -abc;
                }
                return false;
            }
            case 4: {
                const SupportVertex& a = s.v[3];
                const SupportVertex& b = s.v[2];
                const SupportVertex& c = s.v[1];
                const SupportVertex& d = s.v[0];
                Vector3 ao = -a.p;
                Vector3 ab = b.p - a.p;
                Vector3 ac = c.p - a.p;
                Vector3 ad = d.p - a.p;
                Vector3 abc = ab.Cross(ac);
                Vector3 acd = ac.Cross(ad);
                Vector3 adb = ad.Cross(ab);

                if (SameDir(abc, ao)) {
                    s.v[0] = c;
                    s.v[1] = b;
                    s.v[2] = a;
                    s.count = 3;
                    dir = abc;
                    return false;
                }
                if (SameDir(acd, ao)) {
                    s.v[0] = d;
                    s.v[1] = c;
                    s.v[2] = a;
                    s.count = 3;
                    dir = acd;
                    return false;
                }
                if (SameDir(adb, ao)) {
                    s.v[0] = b;
                    s.v[1] = d;
                    s.v[2] = a;
                    s.count = 3;
                    dir = adb;
                    return false;
                }
                return true;
            }
            default:
                return false;
            }
        }

        bool RunGJK(const SupportFn& supportA, const SupportFn& supportB, Simplex& simplex) {
            Vector3 dir(1.0f, 0.0f, 0.0f);
            simplex.v[0] = SupportDiff(supportA, supportB, dir);
            simplex.count = 1;
            dir = -simplex.v[0].p;

            for (int i = 0; i < kMaxGjkIters; ++i) {
                if (dir.LengthSquared() < EPSILON * EPSILON) {
                    dir = Vector3(0.0f, 1.0f, 0.0f);
                }
                SupportVertex next = SupportDiff(supportA, supportB, dir);
                if (next.p.Dot(dir) < 0.0f) {
                    return false;
                }
                simplex.v[simplex.count++] = next;
                if (NextSimplex(simplex, dir)) {
                    return true;
                }
            }
            return false;
        }

        struct EpaFace {
            int i0{ 0 }, i1{ 0 }, i2{ 0 };
            Vector3 n{ 0.0f, 1.0f, 0.0f };
            float dist{ 0.0f };
            bool valid{ true };
        };

        bool ComputeFace(EpaFace& face, const SupportVertex* verts) {
            Vector3 a = verts[face.i0].p;
            Vector3 b = verts[face.i1].p;
            Vector3 c = verts[face.i2].p;
            Vector3 n = (b - a).Cross(c - a);
            float len = n.Length();
            if (len < EPSILON) {
                face.valid = false;
                return false;
            }
            n = n * (1.0f / len);
            float dist = n.Dot(a);
            if (dist < 0.0f) {
                std::swap(face.i1, face.i2);
                n = -n;
                dist = -dist;
            }
            face.n = n;
            face.dist = dist;
            face.valid = true;
            return true;
        }

        bool Barycentric(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& p,
            float& u, float& v, float& w) {
            Vector3 v0 = b - a;
            Vector3 v1 = c - a;
            Vector3 v2 = p - a;
            float d00 = v0.Dot(v0);
            float d01 = v0.Dot(v1);
            float d11 = v1.Dot(v1);
            float d20 = v2.Dot(v0);
            float d21 = v2.Dot(v1);
            float denom = d00 * d11 - d01 * d01;
            if (std::abs(denom) < EPSILON) {
                u = 1.0f;
                v = 0.0f;
                w = 0.0f;
                return false;
            }
            v = (d11 * d20 - d01 * d21) / denom;
            w = (d00 * d21 - d01 * d20) / denom;
            u = 1.0f - v - w;
            return true;
        }

        bool RunEPA(const SupportFn& supportA, const SupportFn& supportB, const Simplex& gjkSimplex,
            Vector3& outNormal, float& outPenetration, Vector3& outContactPoint) {
            if (gjkSimplex.count != 4) return false;

            SupportVertex verts[kMaxEpaVerts];
            int vertCount = 4;
            for (int i = 0; i < 4; ++i) verts[i] = gjkSimplex.v[i];

            EpaFace faces[kMaxEpaFaces];
            int faceCount = 4;
            faces[0] = { 0, 1, 2, {}, 0.0f, true };
            faces[1] = { 0, 2, 3, {}, 0.0f, true };
            faces[2] = { 0, 3, 1, {}, 0.0f, true };
            faces[3] = { 1, 3, 2, {}, 0.0f, true };

            for (int i = 0; i < faceCount; ++i) {
                if (!ComputeFace(faces[i], verts)) return false;
            }

            int bestFace = 0;
            for (int iter = 0; iter < kMaxEpaIters; ++iter) {
                bestFace = -1;
                float minDist = std::numeric_limits<float>::max();
                for (int i = 0; i < faceCount; ++i) {
                    if (!faces[i].valid) continue;
                    if (faces[i].dist < minDist) {
                        minDist = faces[i].dist;
                        bestFace = i;
                    }
                }
                if (bestFace < 0) return false;

                Vector3 n = faces[bestFace].n;
                SupportVertex sv = SupportDiff(supportA, supportB, n);
                float supportDist = sv.p.Dot(n);

                if (supportDist - faces[bestFace].dist < kEpaTol || vertCount >= kMaxEpaVerts) {
                    break;
                }

                int horizonA[kMaxEpaFaces];
                int horizonB[kMaxEpaFaces];
                int horizonCount = 0;
                bool seen[kMaxEpaFaces]{};

                for (int i = 0; i < faceCount; ++i) {
                    if (!faces[i].valid) continue;
                    if (faces[i].n.Dot(sv.p) - faces[i].dist > 0.0f) {
                        faces[i].valid = false;
                        int e[3][2] = {
                            { faces[i].i0, faces[i].i1 },
                            { faces[i].i1, faces[i].i2 },
                            { faces[i].i2, faces[i].i0 }
                        };
                        for (auto& edge : e) {
                            bool reversed = false;
                            for (int h = 0; h < horizonCount; ++h) {
                                if (horizonA[h] == edge[1] && horizonB[h] == edge[0]) {
                                    seen[h] = true;
                                    reversed = true;
                                    break;
                                }
                            }
                            if (!reversed) {
                                horizonA[horizonCount] = edge[0];
                                horizonB[horizonCount] = edge[1];
                                seen[horizonCount] = false;
                                ++horizonCount;
                            }
                        }
                    }
                }

                int newVert = vertCount++;
                verts[newVert] = sv;

                for (int h = 0; h < horizonCount; ++h) {
                    if (seen[h]) continue;
                    if (faceCount >= kMaxEpaFaces) break;
                    EpaFace nf{ horizonA[h], horizonB[h], newVert, {}, 0.0f, true };
                    if (ComputeFace(nf, verts)) {
                        faces[faceCount++] = nf;
                    }
                }
            }

            const EpaFace& face = faces[bestFace];
            Vector3 p0 = verts[face.i0].p;
            Vector3 p1 = verts[face.i1].p;
            Vector3 p2 = verts[face.i2].p;
            Vector3 proj = face.n * face.dist;

            float u, v, w;
            Barycentric(p0, p1, p2, proj, u, v, w);
            u = (std::max)(0.0f, u);
            v = (std::max)(0.0f, v);
            w = (std::max)(0.0f, w);
            float sum = u + v + w;
            if (sum > EPSILON) {
                float inv = 1.0f / sum;
                u *= inv;
                v *= inv;
                w *= inv;
            } else {
                u = 1.0f;
                v = 0.0f;
                w = 0.0f;
            }

            Vector3 pointA = verts[face.i0].a * u + verts[face.i1].a * v + verts[face.i2].a * w;
            Vector3 pointB = verts[face.i0].b * u + verts[face.i1].b * v + verts[face.i2].b * w;

            outNormal = face.n;
            float nLen = outNormal.Length();
            if (nLen < EPSILON) return false;
            outNormal = outNormal * (1.0f / nLen);
            outPenetration = face.dist;
            outContactPoint = (pointA + pointB) * 0.5f;
            return outPenetration > 0.0f;
        }

    } // namespace

    bool TestAndGetContact(
        const SupportFn& supportA,
        const SupportFn& supportB,
        Vector3& outNormal,
        float& outPenetration,
        Vector3& outContactPoint) {
        Simplex simplex;
        if (!RunGJK(supportA, supportB, simplex)) {
            return false;
        }
        return RunEPA(supportA, supportB, simplex, outNormal, outPenetration, outContactPoint);
    }

} // namespace GJK
} // namespace SunvoltumPhysics
