#include <Eigen/Dense>
#include <vector>
#include <fstream>

struct Mesh {
	std::vector<Eigen::Vector3f> verts;
	std::vector<std::vector<unsigned int>> faces;
};


Mesh loadMeshFile(const std::string& filename)
{
	Mesh mesh;

	std::ifstream file(filename);

	std::string line;
	while (!file.eof())
	{
		std::getline(file, line);
		std::stringstream lineSS(line.c_str());
		char lineStart;
		lineSS >> lineStart;
		char ignoreChar;
		if (lineStart == 'v') {
			Eigen::Vector3f v;
			for (int i = 0; i < 3; ++i) lineSS >> v[i];
			mesh.verts.push_back(v);
		}

		if (lineStart == 'f') {
			std::vector<unsigned int> face;
			unsigned int idx, idxTex, idxNorm;
			while (lineSS >> idx >> ignoreChar >> idxTex >> ignoreChar >> idxNorm) {
				face.push_back(idx - 1);
			}
			if (face.size() > 0) mesh.faces.push_back(face);
		}
	}

	return mesh;
}

