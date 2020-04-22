#include "PathTracer.h"

#include <UBL/Image.h>

#include <iostream>

using namespace Ubpa;
using namespace std;

PathTracer::PathTracer(const Scene* scene, const SObj* cam_obj, Image* img)
	: scene{ scene },
	bvh{ const_cast<Scene*>(scene) },
	img{ img },
	cam{ cam_obj->Get<Cmpt::Camera>() },
	ccs{ cam->GenCoordinateSystem(cam_obj->Get<Cmpt::L2W>()->value) }
{

}

void PathTracer::Run() {
	img->SetAll(0.f);

	const size_t spp = 2; // samples per pixel

	for (size_t j = 0; j < img->height; j++) {
		for (size_t i = 0; i < img->width; i++) {
			for (size_t k = 0; k < spp; k++) {
				float u = (i + rand01<float>() - 0.5f) / img->width;
				float v = (j + rand01<float>() - 0.5f) / img->height;
				rayf3 r = cam->GenRay(u, v, ccs);
				rgbf Lo = Trace(r);
				img->At<rgbf>(i, j) += Lo / spp;
			}
		}
		float progress = (j + 1) / float(img->height);
		cout << progress << endl;
	}
}

rgbf PathTracer::Trace(const rayf3& r) {
	// TODO: HW9 - Trace

	/*
	[tips]

	// 1. sample light for-loop
	scene->Each([](const Cmpt::Light* light, const Cmpt::L2W* l2w, const Cmpt::SObjPtr* ptr) {
		SampleLightResult sample_light_rst = SampleLight(light, l2w, ptr);
		// ...
	});

	// 2. query visibility
	bool visibility_rst = visibility.Visit(<BVH>, <ray>);

	// 3. query closest intersection
	IntersectorClosest::Rst closest_rst = closest.Visit(<BVH>, <ray>);

	// 4. sample BRDF
	SampleBRDFResult sample_BRDF_rst = SampleBRDF(<wo>, <intersection>);

	// 5. next ray
	rayf3 next_ray{ <origin>, <dir> }
	*/

	return { 1.f,0.f,1.f };
}

PathTracer::SampleLightResult PathTracer::SampleLight(const Cmpt::Light* light, const Cmpt::L2W* l2w, const Cmpt::SObjPtr* ptr) {
	PathTracer::SampleLightResult rst;
	if (vtable_is<AreaLight>(light->light.get())) {
		auto area_light = static_cast<const AreaLight*>(light->light.get());
		auto geo = ptr->value->Get<Cmpt::Geometry>();
		if (!geo) return rst;
		if (!vtable_is<Square>(geo->primitive.get())) return rst;

		auto Xi = uniform_in_square<float>(); // [0, 1] x [0, 1]
		pointf3 pos_light_space{ 2 * Xi[0] - 1, 0, 2 * Xi[1] - 1 };
		scalef3 s = l2w->WorldScale();
		float area = s[0] * s[1] * Square::side_length * Square::side_length;

		rst.L = area_light->radiance(Xi.cast_to<pointf2>());
		rst.p = 1.f / area;
		rst.x = l2w->value * pos_light_space;
		rst.is_envlight = false;
	}
	else if (vtable_is<EnvLight>(light->light.get())) {
		auto env_light = static_cast<const AreaLight*>(light->light.get());

		auto Xi = uniform_on_sphere<float>();
		pointf3 pos_light_space{ 2 * Xi[0] - 1, 0, 2 * Xi[1] - 1 };
		scalef3 s = l2w->WorldScale();
		float area = s[0] * s[1] * Square::side_length * Square::side_length;

		rst.L = env_light->radiance(Xi.cast_to<normalf>().to_sphere_texcoord());
		rst.p = 1.f / (4 * PI<float>);
		rst.x = pointf3{ std::numeric_limits<float>::max() };
		rst.is_envlight = true;
	}
	return rst;
}

PathTracer::SampleBRDFResult PathTracer::SampleBRDF(const vecf3& wo, IntersectorClosest::Rst intersection) {
	PathTracer::SampleBRDFResult rst;
	auto mat = intersection.sobj->Get<Cmpt::Material>();
	if (!mat) return rst;
	if (!vtable_is<stdBRDF>(mat->material.get())) return rst;
	auto brdf = static_cast<const stdBRDF*>(mat->material.get());

	rgbf albedo = brdf->albedo_factor * brdf->albedo_texture->Sample(intersection.uv).to_rgb();
	//float roughness = brdf->roughness_factor * brdf->roughness_texture->Sample(intersection.uv)[0];
	//float metalness = brdf->metalness_factor * brdf->metalness_texture->Sample(intersection.uv)[0];
	//float alpha = roughness * roughness;

	matf3 surface_to_world = svecf::TBN(intersection.norm.cast_to<vecf3>(), intersection.tangent);
	//auto world_to_surface = surface_to_world.inverse();

	//svecf s_wo = (world_to_surface * wo).cast_to<svecf>();
	svecf s_wi = cos_weighted_on_hemisphere<float>().cast_to<svecf>();
	vecf3 wi = surface_to_world * s_wi.cast_to<vecf3>();

	rst.wi = wi;
	rst.p = s_wi.cos_stheta() / PI<float>;
	rst.brdf = albedo / PI<float>;

	return rst;
}