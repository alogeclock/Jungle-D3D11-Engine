#include "EditorGridPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FEditorGridPass)

FEditorGridPass::FEditorGridPass()
{
	PassType = ERenderPass::EditorGrid;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
