function pctl(arr,n,p,   k) {
  if (n == 0) return "";
  asort(arr);
  k = int((p * n) + 0.999999);
  if (k < 1) k = 1;
  if (k > n) k = n;
  return arr[k];
}

function mean(sum,n) {
  return (n > 0) ? (sum / n) : 0;
}

function std(sum,sum2,n,   v) {
  if (n <= 1) return 0;
  v = (sum2 - (sum * sum) / n) / (n - 1);
  if (v < 0) v = 0;
  return sqrt(v);
}

{
  p = "";
  if (match(FILENAME, /runtime_([a-z0-9_]+)_mon_run[0-9]+\.log/, fm)) {
    p = fm[1];
  }
  if (p == "") next;

  if (match($0, /q_used=([0-9]+)/, m1) &&
      match($0, /q_peak=([0-9]+)/, m2) &&
      match($0, /produced=[0-9]+\(([0-9.]+)Hz\)/, m3) &&
      match($0, /processed=[0-9]+\(([0-9.]+)Hz\)/, m4) &&
      match($0, /dropped=([0-9]+)/, m5) &&
      match($0, /batch_us\(avg\/min\/max\)=([0-9.]+)\//, m6) &&
      match($0, /e2e_us\(avg\/min\/max\)=([0-9.]+)\//, m7)) {

    i = ++n[p];

    q_used[p,i] = m1[1] + 0;
    q_peak[p,i] = m2[1] + 0;
    prod[p,i] = m3[1] + 0.0;
    proc[p,i] = m4[1] + 0.0;
    dropped[p,i] = m5[1] + 0;
    batch[p,i] = m6[1] + 0.0;
    e2e[p,i] = m7[1] + 0.0;

    sum_q_used[p] += q_used[p,i];
    sum_q_used2[p] += q_used[p,i] * q_used[p,i];

    sum_q_peak[p] += q_peak[p,i];
    sum_q_peak2[p] += q_peak[p,i] * q_peak[p,i];

    sum_prod[p] += prod[p,i];
    sum_prod2[p] += prod[p,i] * prod[p,i];

    sum_proc[p] += proc[p,i];
    sum_proc2[p] += proc[p,i] * proc[p,i];

    sum_batch[p] += batch[p,i];
    sum_batch2[p] += batch[p,i] * batch[p,i];

    sum_e2e[p] += e2e[p,i];
    sum_e2e2[p] += e2e[p,i] * e2e[p,i];

    if (i == 1 || dropped[p,i] > dropped_max[p]) dropped_max[p] = dropped[p,i];
  }
}

END {
  print "profile,runs,mon_lines,prod_hz_mean,prod_hz_std,prod_hz_p50,prod_hz_p95,proc_hz_mean,proc_hz_std,proc_hz_p50,proc_hz_p95,dropped_max,q_used_mean,q_used_std,q_used_p50,q_used_p95,q_peak_mean,q_peak_std,q_peak_p50,q_peak_p95,batch_avg_us_mean,batch_avg_us_std,batch_avg_us_p50,batch_avg_us_p95,e2e_avg_us_mean,e2e_avg_us_std,e2e_avg_us_p50,e2e_avg_us_p95";

  order[1] = "prod_os";
  order[2] = "prod_o2";
  order[3] = "prod_o3";
  order[4] = "demo";

  for (o = 1; o <= 4; o++) {
    p = order[o];
    if (n[p] == 0) continue;

    delete a1; delete a2; delete a3; delete a4; delete a5; delete a6;
    for (i = 1; i <= n[p]; i++) {
      a1[i] = prod[p,i];
      a2[i] = proc[p,i];
      a3[i] = q_used[p,i];
      a4[i] = q_peak[p,i];
      a5[i] = batch[p,i];
      a6[i] = e2e[p,i];
    }

    runs = int((n[p] + 24) / 25);

    printf "%s,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
      p,
      runs,
      n[p],
      mean(sum_prod[p], n[p]), std(sum_prod[p], sum_prod2[p], n[p]), pctl(a1, n[p], 0.50), pctl(a1, n[p], 0.95),
      mean(sum_proc[p], n[p]), std(sum_proc[p], sum_proc2[p], n[p]), pctl(a2, n[p], 0.50), pctl(a2, n[p], 0.95),
      dropped_max[p],
      mean(sum_q_used[p], n[p]), std(sum_q_used[p], sum_q_used2[p], n[p]), pctl(a3, n[p], 0.50), pctl(a3, n[p], 0.95),
      mean(sum_q_peak[p], n[p]), std(sum_q_peak[p], sum_q_peak2[p], n[p]), pctl(a4, n[p], 0.50), pctl(a4, n[p], 0.95),
      mean(sum_batch[p], n[p]), std(sum_batch[p], sum_batch2[p], n[p]), pctl(a5, n[p], 0.50), pctl(a5, n[p], 0.95),
      mean(sum_e2e[p], n[p]), std(sum_e2e[p], sum_e2e2[p], n[p]), pctl(a6, n[p], 0.50), pctl(a6, n[p], 0.95);
  }
}
