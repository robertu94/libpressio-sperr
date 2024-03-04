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

  static std::map<std::string, int> const MODES{
    {"bpp", 1},
    {"psnr", 2},
    {"pwe", 3},
  };

class sperr_compressor_plugin : public libpressio_compressor_plugin {
public:
  struct pressio_options get_options_impl() const override
  {
    struct pressio_options options;
    set(options, "sperr:mode", mode);
    set_type(options, "sperr:mode_str", pressio_option_charptr_type);
    set(options, "sperr:tolerance", tol);
    if(mode == 3) set(options, "pressio:abs", tol);
    else set_type(options, "pressio:abs", pressio_option_double_type);
    set(options, "sperr:nthreads", nthreads);
    set(options, "sperr:chunks", pressio_data(chunks.begin(), chunks.end()));

    return options;
  }

  struct pressio_options get_configuration_impl() const override
  {
    struct pressio_options options;
    set(options, "pressio:thread_safe", pressio_thread_safety_multiple);
    set(options, "pressio:stability", "experimental");

    std::vector<std::string> modes_vec;
    modes_vec.reserve(3);
    for (auto const& i : MODES) {
      modes_vec.emplace_back(i.first);
    }
    set(options, "sperr:mode_str", modes_vec);

    

    
        std::vector<std::string> invalidations {"sperr:mode", "sperr:nthreads", "sperr:tolerance", "pressio:abs", "sperr:mode_str", "sperr:chunks"}; 
        std::vector<pressio_configurable const*> invalidation_children {}; 
        
        set(options, "predictors:error_dependent", get_accumulate_configuration("predictors:error_dependent", invalidation_children, invalidations));
        set(options, "predictors:error_agnostic", get_accumulate_configuration("predictors:error_agnostic", invalidation_children, invalidations));
        set(options, "predictors:runtime", get_accumulate_configuration("predictors:runtime", invalidation_children, invalidations));
        set(options, "pressio:highlevel", get_accumulate_configuration("pressio:highlevel", invalidation_children, std::vector<std::string>{"pressio:abs"}));

    return options;
  }

  struct pressio_options get_documentation_impl() const override
  {
    struct pressio_options options;
    set(options, "pressio:description", R"(the SPERR lossless compressor https://github.com/NCAR/SPERR)");
    set(options, "sperr:mode", "mode name");
    set(options, "sperr:mode_str", "mode name");
    set(options, "sperr:chunks", "how to chunk a 3D volume during compression");
    set(options, "sperr:nthreads", "number of threads to use for 3D compression/decompression");
    set(options, "sperr:tolerance", "value for the error bound mode");
    return options;
  }


  int set_options_impl(struct pressio_options const& options) override
  {
    if(get(options, "pressio:abs", &tol) == pressio_options_key_set) {
      mode=3;
    }
    get(options, "sperr:mode", &mode);
    get(options, "sperr:nthreads", &nthreads);
    {
      std::string tmp;
      if(get(options, "sperr:mode_str", &tmp) == pressio_options_key_set) {
        try {
          mode = MODES.at(tmp);
        } catch(std::out_of_range const&) {
          return set_error(1, "unsupported mode: " + tmp);
        }
      }
    }
    get(options, "sperr:tolerance", &tol);
    {
      pressio_data tmp;
      if(get(options, "sperr:chunks", &tmp) == pressio_options_key_set) {
        chunks = tmp.to_vector<size_t>();
      }
    }
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
    int ec = 0;
    switch(dims.size()) {
      case 2: {
        const int inc_header = 1;  // Include a header in 2D bitstreams.
        ec = sperr_comp_2d(input->data(), is_float, dims.at(0), dims.at(1), mode, tol, 
                           inc_header, &bitstream, &stream_len);
        break;
      }
      case 3:
        ec = sperr_comp_3d(input->data(), is_float, dims.at(0), dims.at(1), dims.at(2), chunks.at(0), chunks.at(1), chunks.at(2), mode, tol, nthreads, &bitstream, &stream_len);
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
    std::vector<size_t> outdims(3);
    switch(dims.size()) {
      case 2: {
        const size_t header_len = 10;
        int tmpint = 0;
        sperr_parse_header(input->data(), &outdims[0], &outdims[1], &outdims[2], &tmpint);
        assert(outdims[2] == 1);
        assert(tmpint == is_float);
        ec = sperr_decomp_2d(reinterpret_cast<const uint8_t*>(input->data()) + header_len, 
                             input->size_in_bytes(), is_float, outdims[0], outdims[1], &outbuf);
        break;
      }
      case 3:
        ec = sperr_decomp_3d(input->data(), input->size_in_bytes(), is_float, nthreads, &outdims[0], &outdims[1], &outdims[2], &outbuf);
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
  int32_t mode = 3;
  uint64_t nthreads = 1;
  std::vector<size_t> chunks = {256,256,256};
};

static pressio_register compressor_many_fields_plugin(compressor_plugins(), "sperr", []() {
  return compat::make_unique<sperr_compressor_plugin>();
});

} }

