#include "core/util/prelude.h"
#include "core/conversion/converters/converters.h"

namespace trtorch {
namespace core {
namespace conversion {
namespace converters {
namespace impl {
namespace {

static auto softmax_registrations = RegisterNodeConversionPatterns()
    .pattern({
        "aten::softmax.int(Tensor self, int dim, int? dtype=None) -> (Tensor)",
        [](ConversionCtx* ctx, const torch::jit::Node* n, args& args) -> bool {
            auto in = args[0].ITensor();
            auto shape = util::toVec(in->getDimensions());

            // SoftMax needs at least 4D input
            if (shape.size() < 2) {
                auto new_shape = util::toDimsPad(shape, 2);
                LOG_DEBUG("Input shape is less than 2D got: " << util::toDims(shape) << ", inserting shuffle layer to reshape to 2D tensor shape: " << new_shape);
                auto shuffle = ctx->net->addShuffle(*in);
                shuffle->setReshapeDimensions(new_shape);
                shuffle->setName((util::node_info(n) + " [Reshape to " + util::toStr(new_shape) + ']').c_str());
                in = shuffle->getOutput(0);
            }

            int64_t dim = args[1].IValue()->toInt();
            auto softmax = ctx->net->addSoftMax(*in);

            TRTORCH_CHECK(softmax, "Unable to create softmax layer from node: " << *n);

            if (!softmax) {
                LOG_ERROR("Unable to create softmax layer from node: " << *n);
                return false;
            }
            LOG_WARNING("Disregarding dtype argument, please verify");

            if (shape.size() > 3) {
                softmax->setAxes(1 << (dim));
            } else {
                // When there is no batch dimension
                softmax->setAxes(1 << (dim + 1));
            }

            softmax->setName(util::node_info(n).c_str());
            auto out_value = n->outputs()[0];
            auto out_tensor = softmax->getOutput(0);

            // SoftMax reshape back
            if (shape.size() < 2) {
                auto old_shape = util::toDims(shape);
                LOG_DEBUG("Input shape was less than 2D got: " << old_shape  << ", inserting shuffle layer to reshape back");
                auto shuffle = ctx->net->addShuffle(*out_tensor);
                shuffle->setReshapeDimensions(old_shape);
                shuffle->setName((util::node_info(n) + " [Reshape to " + util::toStr(old_shape) + ']').c_str());
                out_tensor = shuffle->getOutput(0);
            }

            out_tensor->setName(out_value->debugName().c_str());
            ctx->value_tensor_map[out_value] = out_tensor;
            LOG_DEBUG("Output tensor shape: " << out_tensor->getDimensions());

            return true;
        }
    });
} // namespace
} // namespace impl
} // namespace converters
} // namespace conversion
} // namespace core
} // trtorch
