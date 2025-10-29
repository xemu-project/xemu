PATH_TO_QEMU="../../../../../../mips64-linux-user/qemu-mips64"


#
# Bit Count
# ---------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nloc_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nloc_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nloc_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nloc_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nlzc_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nlzc_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nlzc_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nlzc_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pcnt_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pcnt_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pcnt_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pcnt_d_64r6eb

#
# Bit move
# --------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsl_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsl_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsl_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsl_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsr_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsr_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsr_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_binsr_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bmnz_v_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bmz_v_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bsel_v_64r6eb

#
# Bit Set
# -------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bclr_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bclr_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bclr_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bclr_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bneg_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bneg_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bneg_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bneg_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bset_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bset_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bset_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_bset_d_64r6eb

#
# Fixed Multiply
# --------------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_madd_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_madd_q_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddr_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddr_q_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msub_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msub_q_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubr_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubr_q_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mul_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mul_q_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulr_q_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulr_q_w_64r6eb

#
# Float Max Min
# -------------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmax_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmax_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmax_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmax_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmin_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmin_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmin_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_fmin_d_64r6eb

#
# Int Add
# -------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_add_a_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_add_a_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_add_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_add_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_a_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_a_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_adds_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_addv_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_addv_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_addv_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_addv_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hadd_u_d_64r6eb

#
# Int Average
# -----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ave_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_aver_u_d_64r6eb

#
# Int Compare
# -----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ceq_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ceq_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ceq_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ceq_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_cle_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_clt_u_d_64r6eb

#
# Int Divide
# ----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_div_u_d_64r6eb

#
# Int Dot Product
# ---------------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dotp_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpadd_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_dpsub_u_d_64r6eb

#
# Int Max Min
# -----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_a_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_a_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_max_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_a_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_a_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_a_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_a_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_min_u_d_64r6eb

#
# Int Modulo
# ----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mod_u_d_64r6eb

#
# Int Multiply
# ------------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddv_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddv_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddv_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_maddv_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubv_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubv_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubv_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_msubv_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulv_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulv_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulv_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_mulv_d_64r6eb

#
# Int Subtract
# ------------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_asub_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_hsub_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subs_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsus_u_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsus_u_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsus_u_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsus_u_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsuu_s_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsuu_s_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsuu_s_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subsuu_s_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subv_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subv_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subv_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_subv_d_64r6eb

#
# Interleave
# ----------
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvev_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvev_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvev_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvev_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvod_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvod_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvod_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvod_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvl_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvl_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvl_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvl_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvr_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvr_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvr_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_ilvr_d_64r6eb

#
# Logic
# -----
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_and_v_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_nor_v_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_or_v_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_xor_v_64r6eb

#
# Move
# ----
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_move_v_64r6eb

#
# Pack
# ----
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckev_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckev_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckev_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckev_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckod_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckod_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckod_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_pckod_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_vshf_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_vshf_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_vshf_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_vshf_d_64r6eb

#
# Shift
# -----
#
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sll_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sll_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sll_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sll_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sra_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sra_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sra_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_sra_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srar_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srar_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srar_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srar_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srl_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srl_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srl_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srl_d_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srlr_b_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srlr_h_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srlr_w_64r6eb
$PATH_TO_QEMU -cpu I6400  /tmp/test_msa_srlr_d_64r6eb
