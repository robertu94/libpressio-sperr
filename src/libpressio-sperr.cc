#include "std_compat/memory.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/pressio.h"
#include "SPERR_C_API.h"
#include <cmath>

extern "C" void register_libpressio_sperr() {
}

namespace libpressio { namespace sperr_ns {

using namespace ::C_API;

class sperr_compressor_plugin : public libpressio_compressor_plugin {
public:
  struct pressio_options get_options_impl() const override
  {
    struct pressio_options options;
    set(options, "sperr:qlevel", qlev);
    set(options, "sperr:tolerance", tol);
    set(options, "pressio:abs", tol);
    return options;
  }

  struct pressio_options get_configuration_impl() const override
  {
    struct pressio_options options;
    set(options, "pressio:thread_safe", static_cast<int32_t>(pressio_thread_safety_multiple));
    set(options, "pressio:stability", "experimental");
    return options;
  }

  struct pressio_options get_documentation_impl() const override
  {
    struct pressio_options options;
    set(options, "pressio:description", R"(the sperr lossless compressor https://github.com/shaomeng/SPERR)");
    set(options, "sperr:qlevel", "quantization level");
    set(options, "sperr:tolerance", "absolute error tolerance");
    return options;
  }


  int set_options_impl(struct pressio_options const& options) override
  {
    get(options, "pressio:abs", &tol);
    get(options, "sperr:qlevel", &qlev);
    get(options, "sperr:tolerance", &tol);
    return 0;
  }

  int compress_impl(const pressio_data* input,
                    struct pressio_data* output) override
  {
		int is_float = 0;
		switch(input->dtype()) {
			case pressio_float_dtype:
				is_float = 1;
        break;
			case pressio_double_dtype:
				is_float = 0;
        break;
			default:
				return set_error(1, "unsupported datatype");
		}
    auto dims = input->normalized_dims();
    
    void* bitstream = NULL;
    size_t stream_len = 0;
    int ec;
    switch(dims.size()) {
      case 2:
        ec = sperr_qzcomp_2d(input->data(), is_float, dims.at(0), dims.at(1), qlev, tol, &bitstream, &stream_len);
        break;
      case 3:
        ec = sperr_qzcomp_3d(input->data(), is_float, dims.at(0), dims.at(1), dims.at(2), qlev, tol, &bitstream, &stream_len);
        break;
      default:
        return set_error(2, "invalid data dims");
    }
    if(ec != 0) {
      return sperr_compress_error(ec);
    }
    *output = pressio_data::move(pressio_byte_dtype, bitstream, {stream_len}, pressio_data_libc_free_fn, nullptr);

    return 0;
  }

  int decompress_impl(const pressio_data* input,
                      struct pressio_data* output) override
  {
		int is_float = 0;
		switch(output->dtype()) {
			case pressio_float_dtype:
				is_float = 1;
        break;
			case pressio_double_dtype:
				is_float = 0;
        break;
			default:
				return set_error(1, "unsupported datatype");
		}
    

    int ec;
    void* outbuf = NULL; /* Will be free'd later */
    auto dims = output->normalized_dims();
    std::vector<size_t> outdims;
    switch(dims.size()) {
      case 2:
        outdims.resize(2);
        ec = sperr_qzdecomp_2d(input->data(), input->size_in_bytes(), is_float, &outdims[0], &outdims[1], &outbuf);
        break;
      case 3:
        outdims.resize(3);
        ec = sperr_qzdecomp_3d(input->data(), input->size_in_bytes(), is_float, &outdims[0], &outdims[1], &outdims[2], &outbuf);
        break;
      default:
        return set_error(2, "unsupported dims");
    }

    if(ec) {
      return sperr_decompress_error(ec);
    }
    *output = pressio_data::move(output->dtype(), outbuf, outdims, pressio_data_libc_free_fn, nullptr);

    return 0;
  }

  int major_version() const override { return SPERR_VERSION_MAJOR; }
  int minor_version() const override { return SPERR_VERSION_MINOR; }
  int patch_version() const override { return 0; }
  const char* version() const override { return SPERR_GIT_SHA1; }
  const char* prefix() const override { return "sperr"; }

  pressio_options get_metrics_results_impl() const override {
    return {};
  }

  std::shared_ptr<libpressio_compressor_plugin> clone() override
  {
    return compat::make_unique<sperr_compressor_plugin>(*this);
  }

  int sperr_compress_error(int ec) {
    switch(ec) {
      case 1:
        return set_error(5, "compression failed: invalid dst");
      case 2:
        return set_error(6, "compression failed: invalid qlev");
      case 3:
        return set_error(7, "compression failed: invalid dtype");
      default:
        return set_error(8, "compression failed: unknown error");
    }
  }

  int sperr_decompress_error(int ec) {
    switch(ec) {
      case 1:
        return set_error(5, "decompression failed: invalid dst");
      case 2:
        return set_error(7, "decompression failed: invalid dtype");
      default:
        return set_error(8, "decompression failed: unknown error");
    }
  }

  double tol = 1e-6;
  int32_t qlev = 1;
};

static pressio_register compressor_many_fields_plugin(compressor_plugins(), "sperr", []() {
  return compat::make_unique<sperr_compressor_plugin>();
});

} }

