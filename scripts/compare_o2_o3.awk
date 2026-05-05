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
  if (!(p == "prod_o2" || p == "prod_o3")) next;

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
    prod_hz[p,i] = m3[1] + 0.0;
    proc_hz[p,i] = m4[1] + 0.0;
    dropped[p,i] = m5[1] + 0;
    batch[p,i] = m6[1] + 0.0;
    e2e[p,i] = m7[1] + 0.0;

    sum_prod[p] += prod_hz[p,i];
    sum_prod2[p] += prod_hz[p,i] * prod_hz[p,i];

    sum_proc[p] += proc_hz[p,i];
    sum_proc2[p] += proc_hz[p,i] * proc_hz[p,i];

    sum_batch[p] += batch[p,i];
    sum_batch2[p] += batch[p,i] * batch[p,i];

    sum_e2e[p] += e2e[p,i];
    sum_e2e2[p] += e2e[p,i] * e2e[p,i];

    sum_q_used[p] += q_used[p,i];
    sum_q_used2[p] += q_used[p,i] * q_used[p,i];

    sum_q_peak[p] += q_peak[p,i];
    sum_q_peak2[p] += q_peak[p,i] * q_peak[p,i];

    if (i == 1 || dropped[p,i] > dropped_max[p]) dropped_max[p] = dropped[p,i];
  }
}

END {
  print "profile,runs,mon_lines,prod_hz_mean,prod_hz_std,proc_hz_mean,proc_hz_std,batch_avg_us_mean,batch_avg_us_std,batch_avg_us_p50,batch_avg_us_p95,e2e_avg_us_mean,e2e_avg_us_std,e2e_avg_us_p50,e2e_avg_us_p95,q_used_mean,q_used_std,q_peak_mean,q_peak_std,dropped_max";

  order[1] = "prod_o2";
  order[2] = "prod_o3";

  for (o = 1; o <= 2; o++) {
    p = order[o];
    if (n[p] == 0) continue;

    delete a_batch; delete a_e2e;
    for (i = 1; i <= n[p]; i++) {
      a_batch[i] = batch[p,i];
      a_e2e[i] = e2e[p,i];
    }

    runs = int((n[p] + 22) / 23);

    printf "%s,%d,%d,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%d\n",
      p,
      runs,
      n[p],
      mean(sum_prod[p], n[p]), std(sum_prod[p], sum_prod2[p], n[p]),
      mean(sum_proc[p], n[p]), std(sum_proc[p], sum_proc2[p], n[p]),
      mean(sum_batch[p], n[p]), std(sum_batch[p], sum_batch2[p], n[p]), pctl(a_batch, n[p], 0.50), pctl(a_batch, n[p], 0.95),
      mean(sum_e2e[p], n[p]), std(sum_e2e[p], sum_e2e2[p], n[p]), pctl(a_e2e, n[p], 0.50), pctl(a_e2e, n[p], 0.95),
      mean(sum_q_used[p], n[p]), std(sum_q_used[p], sum_q_used2[p], n[p]),
      mean(sum_q_peak[p], n[p]), std(sum_q_peak[p], sum_q_peak2[p], n[p]),
      dropped_max[p];
  }
}
