import json
import os
import re
import ast
import urllib.request

from dotenv import load_dotenv

# Loads HF_TOKEN (and anything else) from your .env file into the environment.
# Must run before anything below tries to read os.environ.
load_dotenv()


class ProjectData:
    def __init__(self, initial_investment, annual_costs, annual_benefits, discount_rate, project_lifespan, intangible_factors=None):
        self.initial_investment = float(initial_investment)
        self.annual_costs = [float(c) for c in annual_costs]
        self.annual_benefits = [float(b) for b in annual_benefits]
        self.discount_rate = float(discount_rate)
        self.project_lifespan = int(project_lifespan)
        self.intangible_factors = intangible_factors or []


class CBAResults:
    def __init__(self):
        self.total_costs_nominal = 0.0
        self.total_benefits_nominal = 0.0
        self.net_present_value = 0.0
        self.benefit_cost_ratio = 0.0
        self.return_on_investment = 0.0
        self.risk_adjusted_npv = 0.0
        self.recommendation = ""


def parse_money(value):
    """
    Convert messy cost values into a float.
    Handles: plain numbers, None, "$40,000+reload", "$1.4 million", "$1.4M", etc.
    Returns None if no numeric value could be recovered.
    """
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)

    text = str(value).lower().strip()

    # "$1.4 million" / "$1.4m"
    million_match = re.search(r"\$?\s*([\d,.]+)\s*(million|m\b)", text)
    if million_match:
        return float(million_match.group(1).replace(",", "")) * 1_000_000

    # "$40,000+reload" / "$40,000" / "40000"
    number_match = re.search(r"[\d,]+(\.\d+)?", text)
    if number_match:
        return float(number_match.group(0).replace(",", ""))

    return None


class DroneValueTable:
    """
    Per-drone-type cost data (cost to produce / cost to intercept),
    hardcoded directly here -- no spreadsheet, no separate file needed.
    Add new drones by adding a row to _RAW_ROWS below.
    """

    # Each row: (name, size, cost_to_produce, cost_to_intercept)
    _RAW_ROWS = [
        ("HSU-001",       "5m",  20000,      10000),
        ("UUV-300",       "13m", 40000,      10000),
        ("HSU-100",       "16m", 300000,     10000),
        ("705 Institute", "17m", 400000,     40000),
        ("AJX-002",       "18m", 500000,     40000),
        ("705 Institute", "45m", 5000000,    1400000),
        ("Second 705",    "45m", 5000000,    1400000),
    ]

    def __init__(self):
        self.table = {}  # name -> {"cost_to_produce": float, "cost_to_intercept": float, "size": str}
        self._build_table()

    def _build_table(self):
        seen_names = set()
        for name, size, produce, intercept in self._RAW_ROWS:
            key = f"{name} ({size})" if name in seen_names else name

            if name in seen_names and name in self.table:
                # Rename the earlier entry too, now that we know it collides
                old_entry = self.table.pop(name)
                self.table[f"{name} ({old_entry['size']})"] = old_entry

            seen_names.add(name)
            self.table[key] = {
                "cost_to_produce": parse_money(produce),
                "cost_to_intercept": parse_money(intercept),
                "size": size,
            }
        print(f"[CBA Agent] Loaded {len(self.table)} hardcoded drone entries")

    def cost_to_produce(self, name, default=0.0):
        entry = self.table.get(name)
        if entry and entry["cost_to_produce"] is not None:
            return entry["cost_to_produce"]
        return default

    def cost_to_intercept(self, name, default=0.0):
        entry = self.table.get(name)
        if entry and entry["cost_to_intercept"] is not None:
            return entry["cost_to_intercept"]
        return default


class CostBenefitAnalysisAgent:
    def __init__(self, results_json_path="tests/fixtures/results.json"):
        self.results_json_path = results_json_path
        # Securely pull the token from the environment (loaded via .env above)
        self.hf_token = os.environ.get("HF_TOKEN", "")
        # Per-drone-type cost lookup -- hardcoded, no file dependency
        self.drone_values = DroneValueTable()

    def gather_project_data(self, user_prompt: str) -> ProjectData:
        print("[CBA Agent] Connecting natively to Hugging Face AI router...")

        system_instructions = (
            "You are a financial entity extraction AI. Analyze the user request and output exactly "
            "a valid Python dictionary with these exact keys: 'initial_investment' (float), "
            "'annual_costs' (list of floats matching lifespan), 'annual_benefits' (list of floats matching lifespan), "
            "'discount_rate' (float), 'project_lifespan' (int). "
            "Do not return any conversational text, explanations, or code blocks. Just the dictionary."
        )

        try:
            if not self.hf_token:
                raise ValueError("HF_TOKEN environment variable is missing.")

            url = "https://router.huggingface.co/v1/chat/completions"
            headers = {
                "Authorization": f"Bearer {self.hf_token}",
                "Content-Type": "application/json"
            }
            payload = {
                "model": "deepseek-ai/DeepSeek-V3-0324:fastest",
                "messages": [
                    {"role": "system", "content": system_instructions},
                    {"role": "user", "content": user_prompt}
                ],
                "temperature": 0.1
            }

            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(url, data=data, headers=headers)

            with urllib.request.urlopen(req, timeout=15) as response:
                response_data = json.loads(response.read().decode("utf-8"))
                raw_text = response_data["choices"][0]["message"]["content"].strip()

            if raw_text.startswith("```"):
                raw_text = raw_text.split("\n", 1)[1].rsplit("\n", 1)[0].strip()
                if raw_text.startswith("python"):
                    raw_text = raw_text.split("\n", 1)[1].strip()

            extracted_data = ast.literal_eval(raw_text)
            print("[CBA Agent] Parameters successfully pulled from native AI response!")

        except Exception as e:
            print(f"[CBA Agent] Native AI call fell back to standard metrics framework. Error context: {e}")
            extracted_data = {
                "initial_investment": 100000.0,
                "annual_costs": [5000.0, 5000.0, 6000.0, 6000.0, 7000.0],
                "annual_benefits": [30000.0, 35000.0, 40000.0, 45000.0, 50000.0],
                "discount_rate": 0.08,
                "project_lifespan": 5
            }

        # Ingest simulation outputs and price them per drone type
        if os.path.exists(self.results_json_path):
            try:
                with open(self.results_json_path, 'r') as f:
                    sim_data = json.load(f)

                metrics = sim_data.get("metrics", {})
                # Expected shape: {"metrics": {"losses": {"HSU-001": 2, "UUV-300": 1, ...},
                #                               "intercepts": {"705 Institute": 1, ...}}}
                losses = metrics.get("losses", {})
                intercepts = metrics.get("intercepts", {})

                loss_penalty = 0.0
                for drone_name, count in losses.items():
                    loss_penalty += count * self.drone_values.cost_to_produce(drone_name)

                intercept_cost = 0.0
                for drone_name, count in intercepts.items():
                    intercept_cost += count * self.drone_values.cost_to_intercept(drone_name)

                total_penalty = loss_penalty + intercept_cost

                # Fallback to the old flat penalty if the sim only reports a scalar total
                if not losses and not intercepts:
                    total_penalty = metrics.get("total_losses", 0) * 5000.0

                lifespan = extracted_data["project_lifespan"]
                for i in range(len(extracted_data["annual_costs"])):
                    extracted_data["annual_costs"][i] += (total_penalty / lifespan)

            except Exception:
                pass

        extracted_data["intangible_factors"] = [
            {"factor": "Autonomous Fleet Mission Readiness Boost", "estimated_yearly_value": 5000.0},
            {"factor": "Environmental Risks", "estimated_yearly_value": -1000.0}
        ]

        return ProjectData(**extracted_data)

    def calculate_metrics(self, data: ProjectData) -> CBAResults:
        results = CBAResults()
        results.total_costs_nominal = sum(data.annual_costs) + data.initial_investment
        results.total_benefits_nominal = sum(data.annual_benefits)

        pv_benefits_minus_costs = 0.0
        pv_total_benefits = 0.0
        pv_total_costs = data.initial_investment

        for t in range(1, data.project_lifespan + 1):
            cost_t = data.annual_costs[t - 1]
            benefit_t = data.annual_benefits[t - 1]
            net_cash_flow = benefit_t - cost_t

            discount_factor = (1 + data.discount_rate) ** t
            pv_benefits_minus_costs += net_cash_flow / discount_factor
            pv_total_benefits += benefit_t / discount_factor
            pv_total_costs += cost_t / discount_factor

        results.net_present_value = pv_benefits_minus_costs - data.initial_investment
        results.benefit_cost_ratio = pv_total_benefits / pv_total_costs
        results.return_on_investment = ((results.total_benefits_nominal - results.total_costs_nominal) / results.total_costs_nominal) * 100

        return results

    def apply_risk_and_intangibles(self, data: ProjectData, results: CBAResults) -> CBAResults:
        total_intangible_impact = 0.0
        for factor in data.intangible_factors:
            total_intangible_impact += factor.get("estimated_yearly_value", 0.0) * data.project_lifespan

        pessimistic_npv = results.net_present_value * 0.85
        results.risk_adjusted_npv = (results.net_present_value * 0.70) + (pessimistic_npv * 0.30) + total_intangible_impact
        return results

    def generate_recommendation(self, results: CBAResults) -> str:
        if results.net_present_value > 0 and results.benefit_cost_ratio > 1.0:
            base_decision = "PROCEED: This simulation deployment configuration is economically optimal."
        elif results.net_present_value <= 0 and results.risk_adjusted_npv > 0:
            base_decision = "PROCEED WITH CAUTION: Baseline financials are thin, but strategic advantages remain clear."
        else:
            base_decision = "REJECT: Project configuration parameters present too much structural cost overhead."

        report = (
            f"==================================================\n"
            f"          COST-BENEFIT ANALYSIS REPORT            \n"
            f"==================================================\n"
            f"Recommendation: {base_decision}\n\n"
            f"--- FINANCIAL METRICS ---\n"
            f"* Total Nominal Costs:      ${results.total_costs_nominal:,.2f}\n"
            f"* Total Nominal Benefits:   ${results.total_benefits_nominal:,.2f}\n"
            f"* Net Present Value (NPV):  ${results.net_present_value:,.2f}\n"
            f"* Benefit-Cost Ratio (BCR): {results.benefit_cost_ratio:.2f}\n"
            f"* Nominal ROI:              {results.return_on_investment:.2f}%\n\n"
            f"--- RISK & INTANGIBLES ANALYSIS ---\n"
            f"* Risk-Adjusted NPV:        ${results.risk_adjusted_npv:,.2f}\n"
        )
        return report

    def run_cba_pipeline(self, user_input_prompt: str):
        try:
            project_data = self.gather_project_data(user_input_prompt)
            initial_metrics = self.calculate_metrics(project_data)
            final_metrics = self.apply_risk_and_intangibles(project_data, initial_metrics)
            report = self.generate_recommendation(final_metrics)
            print(report)
        except Exception as e:
            print(f"Pipeline Execution Failure: {str(e)}")


if __name__ == "__main__":
    sample_prompt = "Evaluate upgrading fleet infrastructure for $120k over a 5 year lifespan with 6k yearly costs and 40k yearly benefits at a 5% discount rate."
    agent = CostBenefitAnalysisAgent()
    agent.run_cba_pipeline(sample_prompt)