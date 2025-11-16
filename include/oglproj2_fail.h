#ifndef OGLPROJ2_H
#define OGLPROJ2_H

#include "oglproj1.h"
#include "oglprojs.h"

namespace oglprojs {

// ============================================================================
// Universl Skeleton System (Forward Kinematics) (failed to meet expectations)
// ============================================================================

class Joint {
	std::string name;
	Transform localTransform;
	Joint *parent = nullptr;
	std::vector<std::shared_ptr<Joint>> children;
	std::shared_ptr<MotionController> motionController;
	std::shared_ptr<Mesh> mesh;

  public:
	Joint(const std::string &name, const Transform &localTrans = Transform()) : name(name), localTransform(localTrans) {}

	void addChild(std::shared_ptr<Joint> child) {
		child->parent = this;
		children.push_back(child);
	}

	void setMotionController(std::shared_ptr<MotionController> controller) { motionController = controller; }

	void setMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }

	// Forward kinematics: compute world transform
	glm::mat4 computeWorldTransform(float time, OrientationType orientType, InterpType interpType) const {
		Transform local = localTransform;

		if (motionController) { local = motionController->evaluate(time, orientType, interpType); }

		glm::mat4 localMatrix = local.toMatrix();

		if (parent) { return parent->computeWorldTransform(time, orientType, interpType) * localMatrix; }

		return localMatrix;
	}

	// Collect all joints for rendering
	void collectJoints(float time, OrientationType orientType, InterpType interpType,
	                   std::vector<std::pair<glm::mat4, std::shared_ptr<Mesh>>> &joints) const {
		glm::mat4 worldTransform = computeWorldTransform(time, orientType, interpType);
		if (mesh) { joints.push_back({worldTransform, mesh}); }

		for (const auto &child : children) { child->collectJoints(time, orientType, interpType, joints); }
	}

	const std::string &getName() const { return name; }
};

class Skeleton {
  public:
	void setRoot(std::shared_ptr<Joint> joint) { root = joint; }

	std::vector<std::pair<glm::mat4, std::shared_ptr<Mesh>>> evaluate(float time) const {
		std::vector<std::pair<glm::mat4, std::shared_ptr<Mesh>>> joints;
		if (root) { root->collectJoints(time, orientType, interpType, joints); }
		return joints;
	}

	OrientationType orientType = OrientationType::Quaternion;
	InterpType interpType = InterpType::CatmullRom;

  private:
	std::shared_ptr<Joint> root;
};
} // namespace oglprojs
#endif // OGLPROJ2_H