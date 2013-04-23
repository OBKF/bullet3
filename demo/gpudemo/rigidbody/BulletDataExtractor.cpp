int NUM_OBJECTS_X = 20;
int NUM_OBJECTS_Y = 20;
int NUM_OBJECTS_Z = 20;



float X_GAP = 2.3f;
float Y_GAP = 2.f;
float Z_GAP = 2.3f;

#include "BulletDataExtractor.h"
#include "Bullet3Serialize/Bullet2FileLoader/btBulletFile.h"
bool keepStaticObjects = true;
extern bool enableExperimentalCpuConcaveCollision;

#include <stdio.h>

#include "OpenGLWindow/OpenGLInclude.h"


#include "OpenGLWindow/GLInstancingRenderer.h"
//#include "LinearMath/btQuickprof.h"
#include "Bullet3Common/b3Quaternion.h"
#include "Bullet3Common/b3Matrix3x3.h"
#include "gpu_narrowphase/host/b3ConvexUtility.h"
#include "OpenGLWindow/ShapeData.h"
#include "../../ObjLoader/objLoader.h"
#include "gpu_rigidbody/host/b3GpuRigidBodyPipeline.h"
#include "gpu_rigidbody/host/b3GpuNarrowPhase.h"

///work-in-progress 
///This ReadBulletSample is kept as simple as possible without dependencies to the Bullet SDK.
///It can be used to load .bullet data for other physics SDKs
///For a more complete example how to load and convert Bullet data using the Bullet SDK check out
///the Bullet/Demos/SerializeDemo and Bullet/Serialize/BulletWorldImporter


//using namespace Bullet;

struct GraphicsVertex
{
	float xyzw[4];
	float normal[3];
	float uv[2];
};
struct GraphicsShape
{
	const float*	m_vertices;
	int				m_numvertices;
	const int*		m_indices;
	int				m_numIndices;
	float			m_scaling[4];
};

struct InstanceGroup
{
	Bullet::btCollisionShapeData* m_shape;
	int		m_collisionShapeIndex;

	b3AlignedObjectArray<bParse::bStructHandle*> m_rigidBodies;
};


void createScene( GLInstancingRenderer& renderer,b3GpuNarrowPhase& np, b3GpuRigidBodyPipeline& rbWorld, const char* fileName)
{
	//const char* fileName="../../bin/convex-trimesh.bullet";
	//const char* fileName="../../bin/1000 convex.bullet";
	//const char* fileName="../../bin/1000 stack.bullet";
	//const char* fileName="../../bin/3000 fall.bullet";
	

	//const char* fileName="../../bin/testFile.bullet";
	

	
	FILE* f = fopen(fileName,"rb");
	if (f)
	{
		fclose(f);
		
		bool verboseDumpAllTypes = false;

		bParse::btBulletFile* bulletFile2 = new bParse::btBulletFile(fileName);

		bool ok = (bulletFile2->getFlags()& bParse::FD_OK)!=0;
	
		if (ok)
			bulletFile2->parse(verboseDumpAllTypes);
		else
		{
			printf("Error loading file %s.\n",fileName);
			exit(0);
		}
		ok = (bulletFile2->getFlags()& bParse::FD_OK)!=0;

		if (!(bulletFile2->getFlags() & bParse::FD_DOUBLE_PRECISION))
		{
			if (!ok)
			{
				printf("Error parsing file %s.\n",fileName);
				exit(0);
			}
	
			if (verboseDumpAllTypes)
			{
				bulletFile2->dumpChunks(bulletFile2->getFileDNA());
			}


			btBulletDataExtractor extractor(renderer,np,rbWorld);
	
			extractor.convertAllObjects(bulletFile2);
			delete bulletFile2;
			return;

		} else
		{
			printf("Error: double precision .bullet files not supported in this demo\n");
		}

		delete bulletFile2;
	} else
	{
		printf("Warning: cannot find file %s, using programmatically created scene instead.\n",fileName);
	}
}



enum LocalBroadphaseNativeTypes
{
	// polyhedral convex shapes
	BOX_SHAPE_PROXYTYPE,
	TRIANGLE_SHAPE_PROXYTYPE,
	TETRAHEDRAL_SHAPE_PROXYTYPE,
	CONVEX_TRIANGLEMESH_SHAPE_PROXYTYPE,
	CONVEX_HULL_SHAPE_PROXYTYPE,
	CONVEX_POINT_CLOUD_SHAPE_PROXYTYPE,
	CUSTOM_POLYHEDRAL_SHAPE_TYPE,
//implicit convex shapes
IMPLICIT_CONVEX_SHAPES_START_HERE,
	SPHERE_SHAPE_PROXYTYPE,
	MULTI_SPHERE_SHAPE_PROXYTYPE,
	CAPSULE_SHAPE_PROXYTYPE,
	CONE_SHAPE_PROXYTYPE,
	CONVEX_SHAPE_PROXYTYPE,
	CYLINDER_SHAPE_PROXYTYPE,
	UNIFORM_SCALING_SHAPE_PROXYTYPE,
	MINKOWSKI_SUM_SHAPE_PROXYTYPE,
	MINKOWSKI_DIFFERENCE_SHAPE_PROXYTYPE,
	BOX_2D_SHAPE_PROXYTYPE,
	CONVEX_2D_SHAPE_PROXYTYPE,
	CUSTOM_CONVEX_SHAPE_TYPE,
//concave shapes
CONCAVE_SHAPES_START_HERE,
	//keep all the convex shapetype below here, for the check IsConvexShape in broadphase proxy!
	TRIANGLE_MESH_SHAPE_PROXYTYPE,
	SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE,
	///used for demo integration FAST/Swift collision library and Bullet
	FAST_CONCAVE_MESH_PROXYTYPE,
	//terrain
	TERRAIN_SHAPE_PROXYTYPE,
///Used for GIMPACT Trimesh integration
	GIMPACT_SHAPE_PROXYTYPE,
///Multimaterial mesh
    MULTIMATERIAL_TRIANGLE_MESH_PROXYTYPE,
	
	EMPTY_SHAPE_PROXYTYPE,
	STATIC_PLANE_PROXYTYPE,
	CUSTOM_CONCAVE_SHAPE_TYPE,
CONCAVE_SHAPES_END_HERE,

	COMPOUND_SHAPE_PROXYTYPE,

	SOFTBODY_SHAPE_PROXYTYPE,
	HFFLUID_SHAPE_PROXYTYPE,
	HFFLUID_BUOYANT_CONVEX_SHAPE_PROXYTYPE,
	INVALID_SHAPE_PROXYTYPE,

	MAX_BROADPHASE_COLLISION_TYPES
	
};

btBulletDataExtractor::btBulletDataExtractor(GLInstancingRenderer&	renderer, b3GpuNarrowPhase& np, b3GpuRigidBodyPipeline& rbWorld)
	:m_renderer(renderer), m_np(np), m_rbPipeline(rbWorld)
{
}

btBulletDataExtractor::~btBulletDataExtractor()
{
}



void btBulletDataExtractor::convertAllObjects(bParse::btBulletFile* bulletFile2)
{
	int i;

	for (i=0;i<bulletFile2->m_collisionShapes.size();i++)
	{
		Bullet::btCollisionShapeData* shapeData = (Bullet::btCollisionShapeData*)bulletFile2->m_collisionShapes[i];
		if (shapeData->m_name)
			printf("converting shape %s\n", shapeData->m_name);
		int shapeIndex = convertCollisionShape(shapeData);
		//valid conversion?
		if (shapeIndex>=0)
		{
			InstanceGroup* group = new InstanceGroup;
			group->m_shape = shapeData;
			group->m_collisionShapeIndex = shapeIndex;
			m_instanceGroups.push_back(group);

		}
	}

	for (i=0;i<bulletFile2->m_rigidBodies.size();i++)
	{
		
		Bullet::btRigidBodyFloatData* colObjData = (Bullet::btRigidBodyFloatData*)bulletFile2->m_rigidBodies[i];
		Bullet::btCollisionShapeData* shapeData = (Bullet::btCollisionShapeData*)colObjData->m_collisionObjectData.m_collisionShape;
		for (int j=0;j<m_instanceGroups.size();j++)
		{
			if (m_instanceGroups[j]->m_shape == shapeData)
			{
				m_instanceGroups[j]->m_rigidBodies.push_back(bulletFile2->m_rigidBodies[i]);
			}
		}
	}

	//now register all objects in order
	for (int i=0;i<m_instanceGroups.size();i++)
	{
		if (m_instanceGroups[i]->m_rigidBodies.size()>0)
		{

			m_renderer.registerShape(m_graphicsShapes[i]->m_vertices,m_graphicsShapes[i]->m_numvertices,m_graphicsShapes[i]->m_indices,m_graphicsShapes[i]->m_numIndices);

			for (int j=0;j<m_instanceGroups[i]->m_rigidBodies.size();j++)
			{
				Bullet::btRigidBodyFloatData* colObjData = (Bullet::btRigidBodyFloatData*)m_instanceGroups[i]->m_rigidBodies[j];
				
				b3Matrix3x3 mat;
				mat.deSerializeFloat((const btMatrix3x3FloatData&)colObjData->m_collisionObjectData.m_worldTransform.m_basis);
				b3Quaternion orn;
				mat.getRotation(orn);
				float quaternion[4] = {orn[0],orn[1],orn[2],orn[3]};
				float pos[4] = {colObjData->m_collisionObjectData.m_worldTransform.m_origin.m_floats[0],
								colObjData->m_collisionObjectData.m_worldTransform.m_origin.m_floats[1],
								colObjData->m_collisionObjectData.m_worldTransform.m_origin.m_floats[2],
								0.f};
				float color[4] = {0,0,0,1};
				float mass = 0.f;
				if (colObjData->m_inverseMass==0.f)
				{
					color[1] = 1;
				} else
				{
					mass = 1.f/colObjData->m_inverseMass;
					color[2] = 1;
				}
				if (keepStaticObjects || colObjData->m_inverseMass!=0.f)
				{
					
					m_rbPipeline.registerPhysicsInstance(mass,pos,quaternion,m_instanceGroups[i]->m_collisionShapeIndex,0,true);
					m_renderer.registerGraphicsInstance(m_instanceGroups[i]->m_collisionShapeIndex,pos,quaternion,color,m_graphicsShapes[i]->m_scaling);
				}

		
				
			}
		}
	}

	for (i=0;i<bulletFile2->m_collisionObjects.size();i++)
	{
		
	}

	

	m_rbPipeline.writeAllInstancesToGpu();
}



int btBulletDataExtractor::convertCollisionShape(  Bullet::btCollisionShapeData* shapeData  )
{
	int shapeIndex = -1;

	switch (shapeData->m_shapeType)
		{
	case STATIC_PLANE_PROXYTYPE:
		{
			Bullet::btStaticPlaneShapeData* planeData = (Bullet::btStaticPlaneShapeData*)shapeData;
			shapeIndex = createPlaneShape(planeData->m_planeNormal,planeData->m_planeConstant, planeData->m_localScaling);
			break;
		}

		case CYLINDER_SHAPE_PROXYTYPE:
		case CAPSULE_SHAPE_PROXYTYPE:
		case BOX_SHAPE_PROXYTYPE:
		case SPHERE_SHAPE_PROXYTYPE:
		case MULTI_SPHERE_SHAPE_PROXYTYPE:
		case CONVEX_HULL_SHAPE_PROXYTYPE:
			{
				Bullet::btConvexInternalShapeData* bsd = (Bullet::btConvexInternalShapeData*)shapeData;
				
				switch (shapeData->m_shapeType)
				{
					case BOX_SHAPE_PROXYTYPE:
						{
							shapeIndex = createBoxShape(bsd->m_implicitShapeDimensions, bsd->m_localScaling,bsd->m_collisionMargin);
							break;
						}
					case SPHERE_SHAPE_PROXYTYPE:
						{
							shapeIndex = createSphereShape(bsd->m_implicitShapeDimensions.m_floats[0],bsd->m_localScaling, bsd->m_collisionMargin);
							break;
						}
					case CONVEX_HULL_SHAPE_PROXYTYPE:
						{
							Bullet::btConvexHullShapeData* convexData = (Bullet::btConvexHullShapeData*)bsd;
							int numPoints = convexData->m_numUnscaledPoints;
							b3Vector3 localScaling;
							localScaling.deSerializeFloat((btVector3FloatData&)bsd->m_localScaling);
							b3AlignedObjectArray<b3Vector3> tmpPoints;
							int i;
							if (convexData->m_unscaledPointsFloatPtr)
							{
								for ( i=0;i<numPoints;i++)
								{
									b3Vector3 pt = b3Vector3(convexData->m_unscaledPointsFloatPtr[i].m_floats[0],
										convexData->m_unscaledPointsFloatPtr[i].m_floats[1],
										convexData->m_unscaledPointsFloatPtr[i].m_floats[2]);//convexData->m_unscaledPointsFloatPtr[i].m_floats[3]);
									
									tmpPoints.push_back(pt*localScaling);
								}
							}
							float unitScaling[4] = {1,1,1,1};


							int strideInBytes = sizeof(b3Vector3);
							strideInBytes = 4*sizeof(float);
							int noHeightField = 1;
							shapeIndex  = m_np.registerConvexHullShape(&tmpPoints[0].m_floats[0],strideInBytes, numPoints,&unitScaling[0]);

							printf("createConvexHull with %d vertices\n",numPoints);

							GraphicsShape* gfxShape = createGraphicsShapeFromConvexHull(&tmpPoints[0],tmpPoints.size());
							m_graphicsShapes.push_back(gfxShape);

							return shapeIndex;
							break;
						}
#if 0
					case CAPSULE_SHAPE_PROXYTYPE:
						{
							btCapsuleShapeData* capData = (btCapsuleShapeData*)shapeData;
							switch (capData->m_upAxis)
							{
							case 0:
								{
									shape = createCapsuleShapeX(implicitShapeDimensions.getY(),2*implicitShapeDimensions.getX());
									break;
								}
							case 1:
								{
									shape = createCapsuleShapeY(implicitShapeDimensions.getX(),2*implicitShapeDimensions.getY());
									break;
								}
							case 2:
								{
									shape = createCapsuleShapeZ(implicitShapeDimensions.getX(),2*implicitShapeDimensions.getZ());
									break;
								}
							default:
								{
									printf("error: wrong up axis for btCapsuleShape\n");
								}

							};
							
							break;
						}
					case CYLINDER_SHAPE_PROXYTYPE:
						{
							btCylinderShapeData* cylData = (btCylinderShapeData*) shapeData;
							b3Vector3 halfExtents = implicitShapeDimensions+margin;
							switch (cylData->m_upAxis)
							{
							case 0:
								{
									shape = createCylinderShapeX(halfExtents.getY(),halfExtents.getX());
									break;
								}
							case 1:
								{
									shape = createCylinderShapeY(halfExtents.getX(),halfExtents.getY());
									break;
								}
							case 2:
								{
									shape = createCylinderShapeZ(halfExtents.getX(),halfExtents.getZ());
									break;
								}
							default:
								{
									printf("unknown Cylinder up axis\n");
								}

							};
							

							
							break;
						}
					case MULTI_SPHERE_SHAPE_PROXYTYPE:
						{
							btMultiSphereShapeData* mss = (btMultiSphereShapeData*)bsd;
							int numSpheres = mss->m_localPositionArraySize;
							int i;
							for ( i=0;i<numSpheres;i++)
							{
								tmpPos[i].deSerializeFloat(mss->m_localPositionArrayPtr[i].m_pos);
								radii[i] = mss->m_localPositionArrayPtr[i].m_radius;
							}
							shape = new btMultiSphereShape(&tmpPos[0],&radii[0],numSpheres);
							break;
						}
			
#endif

					default:
						{
							printf("error: cannot create shape type (%d)\n",shapeData->m_shapeType);
						}
				}

				break;
			}

		case TRIANGLE_MESH_SHAPE_PROXYTYPE:
		{
			Bullet::btTriangleMeshShapeData* trimesh = (Bullet::btTriangleMeshShapeData*)shapeData;
			printf("numparts = %d\n",trimesh->m_meshInterface.m_numMeshParts);
			if (trimesh->m_meshInterface.m_numMeshParts)
			{
				for (int i=0;i<trimesh->m_meshInterface.m_numMeshParts;i++)
				{
					Bullet::btMeshPartData& dat = trimesh->m_meshInterface.m_meshPartsPtr[i];
					printf("numtris = %d, numverts = %d\n", dat.m_numTriangles,dat.m_numVertices);//,dat.m_vertices3f,dat.m_3indices16
					printf("scaling = %f,%f,%f\n", trimesh->m_meshInterface.m_scaling.m_floats[0],trimesh->m_meshInterface.m_scaling.m_floats[1],trimesh->m_meshInterface.m_scaling.m_floats[2]);
					//	dat.
					//dat.

				}
					
				///trimesh->m_meshInterface.m_meshPartsPtr
				//trimesh->m_meshInterface.m_scaling
			}
			//trimesh->m_meshInterface
			//btTriangleIndexVertexArray* meshInterface = createMeshInterface(trimesh->m_meshInterface);
			

			//scaling
			//b3Vector3 scaling; scaling.deSerializeFloat(trimesh->m_meshInterface.m_scaling);
			//meshInterface->setScaling(scaling);

			//printf("trimesh->m_collisionMargin=%f\n",trimesh->m_collisionMargin);
			break;
		}

#if 0
		case COMPOUND_SHAPE_PROXYTYPE:
			{
				btCompoundShapeData* compoundData = (btCompoundShapeData*)shapeData;
				btCompoundShape* compoundShape = createCompoundShape();


				b3AlignedObjectArray<btCollisionShape*> childShapes;
				for (int i=0;i<compoundData->m_numChildShapes;i++)
				{
					btCollisionShape* childShape = convertCollisionShape(compoundData->m_childShapePtr[i].m_childShape);
					if (childShape)
					{
						btTransform localTransform;
						localTransform.deSerializeFloat(compoundData->m_childShapePtr[i].m_transform);
						compoundShape->addChildShape(localTransform,childShape);
					} else
					{
						printf("error: couldn't create childShape for compoundShape\n");
					}
					
				}
				shape = compoundShape;

				break;
			}

			case GIMPACT_SHAPE_PROXYTYPE:
		{
			btGImpactMeshShapeData* gimpactData = (btGImpactMeshShapeData*) shapeData;
			if (gimpactData->m_gimpactSubType == CONST_GIMPACT_TRIMESH_SHAPE)
			{
				btTriangleIndexVertexArray* meshInterface = createMeshInterface(gimpactData->m_meshInterface);
				btGImpactMeshShape* gimpactShape = createGimpactShape(meshInterface);
				b3Vector3 localScaling;
				localScaling.deSerializeFloat(gimpactData->m_localScaling);
				gimpactShape->setLocalScaling(localScaling);
				gimpactShape->setMargin(b3Scalar(gimpactData->m_collisionMargin));
				gimpactShape->updateBound();
				shape = gimpactShape;
			} else
			{
				printf("unsupported gimpact sub type\n");
			}
			break;
		}
		case SOFTBODY_SHAPE_PROXYTYPE:
			{
				return 0;
			}
#endif 
		default:
			{
				printf("unsupported shape type (%d)\n",shapeData->m_shapeType);
			}
		}

		return shapeIndex;
	
}

int btBulletDataExtractor::createBoxShape( const Bullet::btVector3FloatData& halfDimensions, const Bullet::btVector3FloatData& localScaling, float collisionMargin)
{
	float cubeScaling[4] = {
		halfDimensions.m_floats[0]*localScaling.m_floats[0]+collisionMargin,
		halfDimensions.m_floats[1]*localScaling.m_floats[1]+collisionMargin,
		halfDimensions.m_floats[2]*localScaling.m_floats[2]+collisionMargin,
		1};
	int strideInBytes = sizeof(float)*9;
	int noHeightField = 1;
	int cubeCollisionShapeIndex = m_np.registerConvexHullShape(&cube_vertices[0],strideInBytes, sizeof(cube_vertices)/strideInBytes,&cubeScaling[0]);

	{
		int numVertices = sizeof(cube_vertices)/strideInBytes;
		int numIndices = sizeof(cube_indices)/sizeof(int);
		
		GraphicsShape* gfxShape = new GraphicsShape;
		gfxShape->m_vertices = cube_vertices;
		gfxShape->m_numvertices = numVertices;
		gfxShape->m_indices = cube_indices;
		gfxShape->m_numIndices = numIndices;
		for (int i=0;i<4;i++)
			gfxShape->m_scaling[i] = cubeScaling[i];
		m_graphicsShapes.push_back(gfxShape);
	}

	printf("createBoxShape with half extents %f,%f,%f\n",cubeScaling[0], cubeScaling[1],cubeScaling[2]);
	//halfDimensions * localScaling
	return cubeCollisionShapeIndex;
}

int btBulletDataExtractor::createSphereShape( float radius, const Bullet::btVector3FloatData& localScaling, float collisionMargin)
{
	printf("createSphereShape with radius %f\n",radius);
	return -1;
}


int btBulletDataExtractor::createPlaneShape( const Bullet::btVector3FloatData& planeNormal, float planeConstant, const Bullet::btVector3FloatData& localScaling)
{
	printf("createPlaneShape with normal %f,%f,%f and planeConstant\n",planeNormal.m_floats[0], planeNormal.m_floats[1],planeNormal.m_floats[2],planeConstant);
	return -1;
}



GraphicsShape* btBulletDataExtractor::createGraphicsShapeFromWavefrontObj(objLoader* obj)
{
	b3AlignedObjectArray<GraphicsVertex>* vertices = new b3AlignedObjectArray<GraphicsVertex>;
	{
//		int numVertices = obj->vertexCount;
	//	int numIndices = 0;
		b3AlignedObjectArray<int>* indicesPtr = new b3AlignedObjectArray<int>;
			/*
		for (int v=0;v<obj->vertexCount;v++)
		{
			vtx.xyzw[0] = obj->vertexList[v]->e[0];
			vtx.xyzw[1] = obj->vertexList[v]->e[1];
			vtx.xyzw[2] = obj->vertexList[v]->e[2];
			b3Vector3 n(vtx.xyzw[0],vtx.xyzw[1],vtx.xyzw[2]);
			if (n.length2()>SIMD_EPSILON)
			{
				n.normalize();
				vtx.normal[0] = n[0];
				vtx.normal[1] = n[1];
				vtx.normal[2] = n[2];

			} else
			{
				vtx.normal[0] = 0; //todo
				vtx.normal[1] = 1;
				vtx.normal[2] = 0;
			}
			vtx.uv[0] = 0.5f;vtx.uv[1] = 0.5f;	//todo
			vertices->push_back(vtx);
		}
		*/

		for (int f=0;f<obj->faceCount;f++)
		{
			obj_face* face = obj->faceList[f];
			//b3Vector3 normal(face.m_plane[0],face.m_plane[1],face.m_plane[2]);
			if (face->vertex_count>=3)
			{
				b3Vector3 normal(0,1,0);
				int vtxBaseIndex = vertices->size();

				if (face->vertex_count<=4)
				{
					indicesPtr->push_back(vtxBaseIndex);
					indicesPtr->push_back(vtxBaseIndex+1);
					indicesPtr->push_back(vtxBaseIndex+2);
					
					GraphicsVertex vtx0;
					vtx0.xyzw[0] = obj->vertexList[face->vertex_index[0]]->e[0];
					vtx0.xyzw[1] = obj->vertexList[face->vertex_index[0]]->e[1];
					vtx0.xyzw[2] = obj->vertexList[face->vertex_index[0]]->e[2];
					vtx0.uv[0] = 0.5;
					vtx0.uv[1] = 0.5;

					GraphicsVertex vtx1;
					vtx1.xyzw[0] = obj->vertexList[face->vertex_index[1]]->e[0];
					vtx1.xyzw[1] = obj->vertexList[face->vertex_index[1]]->e[1];
					vtx1.xyzw[2] = obj->vertexList[face->vertex_index[1]]->e[2];
					vtx1.uv[0] = 0.5;
					vtx1.uv[1] = 0.5;

					GraphicsVertex vtx2;
					vtx2.xyzw[0] = obj->vertexList[face->vertex_index[2]]->e[0];
					vtx2.xyzw[1] = obj->vertexList[face->vertex_index[2]]->e[1];
					vtx2.xyzw[2] = obj->vertexList[face->vertex_index[2]]->e[2];
					vtx2.uv[0] = 0.5;
					vtx2.uv[1] = 0.5;

					b3Vector3 v0(vtx0.xyzw[0],vtx0.xyzw[1],vtx0.xyzw[2]);
					b3Vector3 v1(vtx1.xyzw[0],vtx1.xyzw[1],vtx1.xyzw[2]);
					b3Vector3 v2(vtx2.xyzw[0],vtx2.xyzw[1],vtx2.xyzw[2]);

					normal = (v1-v0).cross(v2-v0);
					normal.normalize();
					vtx0.normal[0] = normal[0];
					vtx0.normal[1] = normal[1];
					vtx0.normal[2] = normal[2];
					vtx1.normal[0] = normal[0];
					vtx1.normal[1] = normal[1];
					vtx1.normal[2] = normal[2];
					vtx2.normal[0] = normal[0];
					vtx2.normal[1] = normal[1];
					vtx2.normal[2] = normal[2];
					vertices->push_back(vtx0);
					vertices->push_back(vtx1);
					vertices->push_back(vtx2);
				}
				if (face->vertex_count==4)
				{

					indicesPtr->push_back(vtxBaseIndex);
					indicesPtr->push_back(vtxBaseIndex+1);
					indicesPtr->push_back(vtxBaseIndex+2);
					indicesPtr->push_back(vtxBaseIndex+3);
//
					GraphicsVertex vtx3;
					vtx3.xyzw[0] = obj->vertexList[face->vertex_index[3]]->e[0];
					vtx3.xyzw[1] = obj->vertexList[face->vertex_index[3]]->e[1];
					vtx3.xyzw[2] = obj->vertexList[face->vertex_index[3]]->e[2];
					vtx3.uv[0] = 0.5;
					vtx3.uv[1] = 0.5;

					vtx3.normal[0] = normal[0];
					vtx3.normal[1] = normal[1];
					vtx3.normal[2] = normal[2];

					vertices->push_back(vtx3);

				}
			}
		}
		
		
		GraphicsShape* gfxShape = new GraphicsShape;
		gfxShape->m_vertices = &vertices->at(0).xyzw[0];
		gfxShape->m_numvertices = vertices->size();
		gfxShape->m_indices = &indicesPtr->at(0);
		gfxShape->m_numIndices = indicesPtr->size();
		for (int i=0;i<4;i++)
			gfxShape->m_scaling[i] = 1;//bake the scaling into the vertices 
		return gfxShape;
	}
}


GraphicsShape* btBulletDataExtractor::createGraphicsShapeFromConvexHull(const b3Vector3* tmpPoints, int numPoints)
{
	b3ConvexUtility* utilPtr = new b3ConvexUtility();
	bool merge = true;
	utilPtr->initializePolyhedralFeatures(tmpPoints,numPoints,merge);
	
	b3AlignedObjectArray<GraphicsVertex>* vertices = new b3AlignedObjectArray<GraphicsVertex>;
	{
		int numVertices = utilPtr->m_vertices.size();
		int numIndices = 0;
		b3AlignedObjectArray<int>* indicesPtr = new b3AlignedObjectArray<int>;
		for (int f=0;f<utilPtr->m_faces.size();f++)
		{
			const btMyFace& face = utilPtr->m_faces[f];
			b3Vector3 normal(face.m_plane[0],face.m_plane[1],face.m_plane[2]);
			if (face.m_indices.size()>2)
			{
				
				GraphicsVertex vtx;
				const b3Vector3& orgVertex = utilPtr->m_vertices[face.m_indices[0]];
				vtx.xyzw[0] = orgVertex[0];vtx.xyzw[1] = orgVertex[1];vtx.xyzw[2] = orgVertex[2];vtx.xyzw[3] = 0.f;
				vtx.normal[0] = normal[0];vtx.normal[1] = normal[1];vtx.normal[2] = normal[2];
				vtx.uv[0] = 0.5f;vtx.uv[1] = 0.5f;
				int newvtxindex0 = vertices->size();
				vertices->push_back(vtx);
			
				for (int j=1;j<face.m_indices.size()-1;j++)
				{
					indicesPtr->push_back(newvtxindex0);
					{
						GraphicsVertex vtx;
						const b3Vector3& orgVertex = utilPtr->m_vertices[face.m_indices[j]];
						vtx.xyzw[0] = orgVertex[0];vtx.xyzw[1] = orgVertex[1];vtx.xyzw[2] = orgVertex[2];vtx.xyzw[3] = 0.f;
						vtx.normal[0] = normal[0];vtx.normal[1] = normal[1];vtx.normal[2] = normal[2];
						vtx.uv[0] = 0.5f;vtx.uv[1] = 0.5f;
						int newvtxindexj = vertices->size();
						vertices->push_back(vtx);
						indicesPtr->push_back(newvtxindexj);
					}

					{
						GraphicsVertex vtx;
						const b3Vector3& orgVertex = utilPtr->m_vertices[face.m_indices[j+1]];
						vtx.xyzw[0] = orgVertex[0];vtx.xyzw[1] = orgVertex[1];vtx.xyzw[2] = orgVertex[2];vtx.xyzw[3] = 0.f;
						vtx.normal[0] = normal[0];vtx.normal[1] = normal[1];vtx.normal[2] = normal[2];
						vtx.uv[0] = 0.5f;vtx.uv[1] = 0.5f;
						int newvtxindexj1 = vertices->size();
						vertices->push_back(vtx);
						indicesPtr->push_back(newvtxindexj1);
					}
				}
			}
		}
		
		
		GraphicsShape* gfxShape = new GraphicsShape;
		gfxShape->m_vertices = &vertices->at(0).xyzw[0];
		gfxShape->m_numvertices = vertices->size();
		gfxShape->m_indices = &indicesPtr->at(0);
		gfxShape->m_numIndices = indicesPtr->size();
		for (int i=0;i<4;i++)
			gfxShape->m_scaling[i] = 1;//bake the scaling into the vertices 
		return gfxShape;
	}
}