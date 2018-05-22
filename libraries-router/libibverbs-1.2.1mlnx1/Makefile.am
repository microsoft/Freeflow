AM_CPPFLAGS = -I$(srcdir)/include

lib_LTLIBRARIES = src/libibverbs.la

ACLOCAL_AMFLAGS = -I config
AM_CFLAGS = -g -Wall -Werror

src_libibverbs_la_CFLAGS = $(AM_CFLAGS) -DIBV_CONFIG_DIR=\"$(sysconfdir)/libibverbs.d\" \
			   $(LIBNL_CFLAGS)

libibverbs_version_script = @LIBIBVERBS_VERSION_SCRIPT@

src_libibverbs_la_SOURCES = src/cmd.c src/compat-1_0.c src/device.c src/init.c \
			    src/marshall.c src/memory.c src/sysfs.c src/verbs.c \
			    src/enum_strs.c src/cmd_exp.c
if ! NO_RESOLVE_NEIGH
src_libibverbs_la_SOURCES += src/neigh.c
noinst_HEADERS = src/neigh.h
endif
src_libibverbs_la_LDFLAGS = -version-info 1 -export-dynamic $(LIBNL_LIBS)\
    $(libibverbs_version_script)
src_libibverbs_la_DEPENDENCIES = $(srcdir)/src/libibverbs.map

bin_PROGRAMS = examples/ibv_devices examples/ibv_devinfo \
    examples/ibv_asyncwatch examples/ibv_rc_pingpong examples/ibv_uc_pingpong \
    examples/ibv_ud_pingpong examples/ibv_srq_pingpong examples/ibv_shared_mr \
    examples/ibv_xsrq_pingpong examples/ibv_cc_pingpong examples/ibv_task_pingpong \
    examples/ibv_dcini examples/ibv_dctgt examples/ibv_umr examples/ibv_intf \
    examples/ibv_polldcinfo

examples_ibv_devices_SOURCES = examples/device_list.c
examples_ibv_devices_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_devinfo_SOURCES = examples/devinfo.c
examples_ibv_devinfo_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_rc_pingpong_SOURCES = examples/rc_pingpong.c examples/pingpong.c
examples_ibv_rc_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_uc_pingpong_SOURCES = examples/uc_pingpong.c examples/pingpong.c
examples_ibv_uc_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_ud_pingpong_SOURCES = examples/ud_pingpong.c examples/pingpong.c
examples_ibv_ud_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_srq_pingpong_SOURCES = examples/srq_pingpong.c examples/pingpong.c
examples_ibv_srq_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_shared_mr_SOURCES = examples/shared_mr.c
examples_ibv_shared_mr_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_asyncwatch_SOURCES = examples/asyncwatch.c
examples_ibv_asyncwatch_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_xsrq_pingpong_SOURCES = examples/xsrq_pingpong.c examples/pingpong.c
examples_ibv_xsrq_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_cc_pingpong_SOURCES = examples/cc_pingpong.c
examples_ibv_cc_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_task_pingpong_SOURCES = examples/task_pingpong.c
examples_ibv_task_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_dcini_SOURCES = examples/dcini.c
examples_ibv_dcini_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_dctgt_SOURCES = examples/dctgt.c examples/pingpong.c examples/dc.h
examples_ibv_dctgt_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_umr_SOURCES = examples/umr_rc.c examples/pingpong.c
examples_ibv_umr_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_intf_SOURCES = examples/intf.c examples/get_clock.c examples/get_clock.h
examples_ibv_intf_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_polldcinfo_SOURCES = examples/polldcinfo.c
examples_ibv_polldcinfo_LDADD = $(top_builddir)/src/libibverbs.la


# Enable ibverbs library test
#
if TEST_ENABLE

# Primary ibverbs test
#
TESTS = ibv_test
noinst_PROGRAMS = ibv_test

ibv_test_CXXFLAGS = -g -Wall -O3 -fno-strict-aliasing
ibv_test_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/tests \
	-I$(top_srcdir)/tests/cmn
ibv_test_SOURCES = \
	tests/cmn/gtest_cmn.h \
	tests/gtest/gtest.h \
	tests/api/gtest_cd.h \
	tests/gtest_main.cc \
	tests/cmn/gtest_cmn.cc \
	tests/api/gtest_cd_tc1.cc \
	tests/api/gtest_cd_tc2.cc \
	tests/api/gtest_cd_tc3.cc \
	tests/api/gtest_cd_tc4.cc \
	tests/api/gtest_cd_tc5.cc \
	tests/api/gtest_cd_tc6.cc \
	tests/api/gtest_cd_tc7.cc \
	tests/api/gtest_cd_tc8.cc \
	tests/api/gtest_init.cc
ibv_test_LDADD = \
	$(top_builddir)/src/libibverbs.la

test: ibv_test
	rm -f core.*
	./ibv_test

valgrind: ibv_test
	valgrind --tool=memcheck --leak-check=full --track-origins=yes ./ibv_test

endif

libibverbsincludedir = $(includedir)/infiniband

libibverbsinclude_HEADERS = include/infiniband/arch.h include/infiniband/driver.h \
    include/infiniband/kern-abi.h include/infiniband/opcode.h include/infiniband/verbs.h \
    include/infiniband/sa-kern-abi.h include/infiniband/sa.h include/infiniband/marshall.h \
    include/infiniband/ofa_verbs.h include/infiniband/driver_exp.h include/infiniband/kern-abi_exp.h \
    include/infiniband/verbs_exp.h include/infiniband/peer_ops.h

man_MANS = man/ibv_asyncwatch.1 man/ibv_devices.1 man/ibv_devinfo.1	\
    man/ibv_rc_pingpong.1 man/ibv_uc_pingpong.1 man/ibv_ud_pingpong.1	\
    man/ibv_srq_pingpong.1 man/ibv_alloc_pd.3 man/ibv_attach_mcast.3	\
    man/ibv_create_ah.3 man/ibv_create_ah_from_wc.3			\
    man/ibv_create_comp_channel.3 man/ibv_create_cq.3			\
    man/ibv_create_qp.3 man/ibv_create_srq.3 man/ibv_event_type_str.3	\
    man/ibv_fork_init.3 man/ibv_get_async_event.3			\
    man/ibv_get_cq_event.3 man/ibv_get_device_guid.3			\
    man/ibv_get_device_list.3 man/ibv_get_device_name.3			\
    man/ibv_modify_qp.3 man/ibv_modify_srq.3 man/ibv_open_device.3	\
    man/ibv_poll_cq.3 man/ibv_post_recv.3 man/ibv_post_send.3		\
    man/ibv_post_srq_recv.3 man/ibv_query_device.3 man/ibv_query_gid.3	\
    man/ibv_query_pkey.3 man/ibv_query_port.3 man/ibv_query_qp.3	\
    man/ibv_query_srq.3 man/ibv_rate_to_mult.3 man/ibv_reg_mr.3		\
    man/ibv_req_notify_cq.3 man/ibv_resize_cq.3 man/ibv_rate_to_mbps.3	\
    man/verbs.7 man/ibv_exp_reg_shared_mr.3 man/ibv_shared_mr.1		\
    man/ibv_exp_post_task.3 man/ibv_exp_modify_cq.3			\
    man/ibv_cc_pingpong.1 man/ibv_task_pingpong.1 man/ibv_create_qp_ex.3 \
    man/ibv_create_srq_ex.3 man/ibv_open_xrcd.3 man/ibv_get_srq_num.3    \
    man/ibv_open_qp.3 man/ibv_create_flow.3 man/ibv_exp_create_flow.3	\
    man/ibv_exp_create_cq.3 man/ibv_exp_modify_qp.3			\
    man/ibv_exp_poll_cq.3 man/ibv_exp_query_device.3			\
    man/ibv_exp_get_provider_func.3 man/ibv_exp_reg_mr.3		\
    man/ibv_exp_bind_mw.3 man/ibv_exp_create_qp.3			\
    man/ibv_exp_post_send.3 man/ibv_exp_create_dct.3			\
    man/ibv_exp_prefetch_mr.3 man/ibv_exp_alloc_mkey_list_memory.3      \
    man/ibv_exp_create_mr.3 man/ibv_exp_dealloc_mkey_list_memory.3      \
    man/ibv_exp_query_mkey.3 man/ibv_exp_query_dct.3 			\
    man/ibv_exp_rereg_mr.3 man/ibv_exp_query_values.3			\
    man/ibv_alloc_mw.3 man/ibv_intf.1 man/ibv_exp_create_res_domain.3 \
    man/ibv_exp_query_intf.3 man/ibv_exp_create_wq.3                    \
    man/ibv_exp_modify_wq.3 man/ibv_exp_create_rwq_ind_table.3          \
    man/ibv_exp_query_gid_attr.3 man/ibv_exp_alloc_ec_calc.3		\
    man/ibv_exp_ec_encode_async.3 man/ibv_exp_ec_decode_async.3		\
    man/ibv_exp_ec_update_async.3					\
    man/peer_direct.7 man/ibv_exp_peer_commit_qp.3			\
    man/ibv_query_device_ex.3 man/ibv_rereg_mr.3 man/ibv_bind_mw.3	\
    man/ibv_inc_rkey.3

EXTRA_DIST = include/infiniband/driver.h include/infiniband/kern-abi.h \
    include/infiniband/opcode.h include/infiniband/verbs.h include/infiniband/marshall.h \
    include/infiniband/sa-kern-abi.h include/infiniband/sa.h \
    src/ibverbs.h \
    tests/gtest/gtest-all.cc \
    examples/pingpong.h examples/cc_pingpong.h \
    src/libibverbs.map libibverbs.spec.in $(man_MANS)
EXTRA_DIST += debian
EXTRA_DIST += autogen.sh


dist-hook: libibverbs.spec
	cp libibverbs.spec $(distdir)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man3 && \
	$(RM) ibv_ack_async_event.3 && \
	$(RM) ibv_ack_cq_events.3 && \
	$(RM) ibv_close_device.3 && \
	$(RM) ibv_dealloc_pd.3 && \
	$(RM) ibv_dereg_mr.3 && \
	$(RM) ibv_destroy_ah.3 && \
	$(RM) ibv_destroy_comp_channel.3 && \
	$(RM) ibv_destroy_cq.3 && \
	$(RM) ibv_destroy_qp.3 && \
	$(RM) ibv_destroy_srq.3 && \
	$(RM) ibv_detach_mcast.3 && \
	$(RM) ibv_free_device_list.3 && \
	$(RM) ibv_init_ah_from_wc.3 && \
	$(RM) ibv_node_type_str.3 && \
	$(RM) ibv_port_state_str.3 && \
	$(RM) ibv_close_xrcd.3 && \
	$(RM) ibv_destroy_flow.3 && \
	$(RM) ibv_dealloc_mw.3 && \
	$(RM) ibv_exp_destroy_flow.3 && \
	$(RM) ibv_exp_destroy_res_domain.3 && \
	$(RM) ibv_exp_release_intf.3 && \
	$(RM) ibv_exp_destroy_wq.3 && \
	$(RM) ibv_exp_destroy_rwq_ind_table.3 && \
	$(RM) ibv_exp_dealloc_ec_calc.3 && \
	$(RM) ibv_exp_ec_encode_sync.3 && \
	$(RM) ibv_exp_ec_decode_sync.3 && \
	$(RM) ibv_exp_ec_update_sync.3 && \
	$(RM) ibv_exp_rollback_qp.3 && \
	$(RM) ibv_exp_peer_peek_cq.3 && \
	$(RM) ibv_exp_peer_abort_peek_cq.3 && \
	$(LN_S) ibv_get_async_event.3 ibv_ack_async_event.3 && \
	$(LN_S) ibv_get_cq_event.3 ibv_ack_cq_events.3 && \
	$(LN_S) ibv_open_device.3 ibv_close_device.3 && \
	$(LN_S) ibv_alloc_pd.3 ibv_dealloc_pd.3 && \
	$(LN_S) ibv_reg_mr.3 ibv_dereg_mr.3 && \
	$(LN_S) ibv_create_ah.3 ibv_destroy_ah.3 && \
	$(LN_S) ibv_create_comp_channel.3 ibv_destroy_comp_channel.3 && \
	$(LN_S) ibv_create_cq.3 ibv_destroy_cq.3 && \
	$(LN_S) ibv_create_qp.3 ibv_destroy_qp.3 && \
	$(LN_S) ibv_create_srq.3 ibv_destroy_srq.3 && \
	$(LN_S) ibv_attach_mcast.3 ibv_detach_mcast.3 && \
	$(LN_S) ibv_get_device_list.3 ibv_free_device_list.3 && \
	$(LN_S) ibv_create_ah_from_wc.3 ibv_init_ah_from_wc.3 && \
	$(LN_S) ibv_event_type_str.3 ibv_node_type_str.3 && \
	$(LN_S) ibv_event_type_str.3 ibv_port_state_str.3 && \
	$(LN_S) ibv_open_xrcd.3 ibv_close_xrcd.3 && \
	$(LN_S) ibv_create_flow.3 ibv_destroy_flow.3 && \
	$(LN_S) ibv_exp_create_flow.3 ibv_exp_destroy_flow.3 && \
	$(LN_S) ibv_alloc_mw.3 ibv_dealloc_mw.3 && \
	$(LN_S) ibv_exp_create_res_domain.3 ibv_exp_destroy_res_domain.3 && \
	$(LN_S) ibv_exp_query_intf.3 ibv_exp_release_intf.3 && \
	$(LN_S) ibv_exp_create_wq.3 ibv_exp_destroy_wq.3 && \
	$(LN_S) ibv_exp_create_rwq_ind_table.3 ibv_exp_destroy_rwq_ind_table.3 && \
	$(LN_S) ibv_exp_alloc_ec_calc.3 ibv_exp_dealloc_ec_calc.3 && \
	$(LN_S) ibv_exp_ec_encode_async.3 ibv_exp_ec_encode_sync.3 && \
	$(LN_S) ibv_exp_ec_decode_async.3 ibv_exp_ec_decode_sync.3 && \
	$(LN_S) ibv_exp_ec_update_async.3 ibv_exp_ec_update_sync.3 && \
	$(LN_S) ibv_exp_peer_commit_qp.3 ibv_exp_rollback_qp.3 && \
	$(LN_S) ibv_exp_peer_commit_qp.3 ibv_exp_peer_peek_cq.3 && \
	$(LN_S) ibv_exp_peer_commit_qp.3 ibv_exp_peer_abort_peek_cq.3
