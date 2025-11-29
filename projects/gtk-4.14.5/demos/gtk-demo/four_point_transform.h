#pragma once

#include <graphene.h>

void perspective_3d (graphene_point3d_t *p1,
                     graphene_point3d_t *p2,
                     graphene_point3d_t *p3,
                     graphene_point3d_t *p4,
                     graphene_point3d_t *q1,
                     graphene_point3d_t *q2,
                     graphene_point3d_t *q3,
                     graphene_point3d_t *q4,
                     graphene_matrix_t  *m);
